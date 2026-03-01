# VeloZ Production Environment
# Terraform configuration for production infrastructure

terraform {
  required_version = ">= 1.5.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = ">= 5.0"
    }
    kubernetes = {
      source  = "hashicorp/kubernetes"
      version = ">= 2.20"
    }
    helm = {
      source  = "hashicorp/helm"
      version = ">= 2.10"
    }
  }

  backend "s3" {
    bucket         = "veloz-terraform-state"
    key            = "production/terraform.tfstate"
    region         = "us-east-1"
    encrypt        = true
    dynamodb_table = "veloz-terraform-locks"
  }
}

provider "aws" {
  region = var.aws_region

  default_tags {
    tags = {
      Project     = "veloz"
      Environment = "production"
      ManagedBy   = "terraform"
    }
  }
}

# Local variables
locals {
  project     = "veloz"
  environment = "production"

  tags = {
    Project     = local.project
    Environment = local.environment
    ManagedBy   = "terraform"
  }
}

# VPC Module
module "vpc" {
  source = "../../modules/vpc"

  project     = local.project
  environment = local.environment

  vpc_cidr              = "10.0.0.0/16"
  public_subnet_cidrs   = ["10.0.1.0/24", "10.0.2.0/24", "10.0.3.0/24"]
  private_subnet_cidrs  = ["10.0.11.0/24", "10.0.12.0/24", "10.0.13.0/24"]
  database_subnet_cidrs = ["10.0.21.0/24", "10.0.22.0/24", "10.0.23.0/24"]

  enable_nat_gateway = true
  single_nat_gateway = false  # HA NAT gateways for production

  enable_flow_logs         = true
  flow_logs_retention_days = 90

  tags = local.tags
}

# EKS Module
module "eks" {
  source = "../../modules/eks"

  project     = local.project
  environment = local.environment

  vpc_id          = module.vpc.vpc_id
  subnet_ids      = concat(module.vpc.public_subnet_ids, module.vpc.private_subnet_ids)
  node_subnet_ids = module.vpc.private_subnet_ids

  cluster_version = "1.29"

  endpoint_private_access = true
  endpoint_public_access  = true
  public_access_cidrs     = var.allowed_cidr_blocks

  enabled_cluster_log_types  = ["api", "audit", "authenticator", "controllerManager", "scheduler"]
  cluster_log_retention_days = 90

  node_groups = {
    general = {
      instance_types             = ["t3.large"]
      capacity_type              = "ON_DEMAND"
      disk_size                  = 100
      desired_size               = 3
      min_size                   = 2
      max_size                   = 10
      max_unavailable_percentage = 25
      labels = {
        "workload-type" = "general"
      }
      taints = []
    }
    trading = {
      instance_types             = ["c6i.xlarge"]
      capacity_type              = "ON_DEMAND"
      disk_size                  = 100
      desired_size               = 2
      min_size                   = 1
      max_size                   = 5
      max_unavailable_percentage = 50
      labels = {
        "workload-type" = "trading"
      }
      taints = [
        {
          key    = "workload-type"
          value  = "trading"
          effect = "NO_SCHEDULE"
        }
      ]
    }
  }

  enable_ebs_csi_driver = true

  tags = local.tags
}

# RDS Module
module "rds" {
  source = "../../modules/rds"

  project     = local.project
  environment = local.environment

  vpc_id               = module.vpc.vpc_id
  db_subnet_group_name = module.vpc.db_subnet_group_name

  allowed_security_groups = [module.eks.node_security_group_id]
  allowed_cidr_blocks     = []

  engine_version       = "16"
  engine_minor_version = "1"
  instance_class       = "db.r6g.large"

  allocated_storage     = 100
  max_allocated_storage = 500
  storage_type          = "gp3"

  database_name   = "veloz"
  master_username = "veloz"

  multi_az            = true
  deletion_protection = true
  skip_final_snapshot = false

  backup_retention_period = 30
  backup_window           = "03:00-04:00"
  maintenance_window      = "Mon:04:00-Mon:05:00"

  performance_insights_enabled          = true
  performance_insights_retention_period = 7

  monitoring_interval = 60

  create_read_replica    = true
  replica_instance_class = "db.r6g.large"

  tags = local.tags
}

# ElastiCache Redis
resource "aws_elasticache_replication_group" "main" {
  replication_group_id = "${local.project}-${local.environment}"
  description          = "Redis cluster for ${local.project} ${local.environment}"

  node_type            = "cache.r6g.large"
  num_cache_clusters   = 2
  port                 = 6379
  parameter_group_name = "default.redis7"
  engine_version       = "7.0"

  subnet_group_name  = module.vpc.elasticache_subnet_group_name
  security_group_ids = [aws_security_group.redis.id]

  automatic_failover_enabled = true
  multi_az_enabled           = true

  at_rest_encryption_enabled = true
  transit_encryption_enabled = true
  auth_token                 = random_password.redis_auth.result

  snapshot_retention_limit = 7
  snapshot_window          = "05:00-06:00"
  maintenance_window       = "sun:06:00-sun:07:00"

  auto_minor_version_upgrade = true

  tags = local.tags
}

resource "random_password" "redis_auth" {
  length  = 32
  special = false
}

resource "aws_security_group" "redis" {
  name        = "${local.project}-${local.environment}-redis-sg"
  description = "Security group for ElastiCache Redis"
  vpc_id      = module.vpc.vpc_id

  ingress {
    description     = "Redis from EKS nodes"
    from_port       = 6379
    to_port         = 6379
    protocol        = "tcp"
    security_groups = [module.eks.node_security_group_id]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }

  tags = merge(local.tags, {
    Name = "${local.project}-${local.environment}-redis-sg"
  })
}

# Store Redis credentials in Secrets Manager
resource "aws_secretsmanager_secret" "redis_credentials" {
  name        = "${local.project}/${local.environment}/redis/credentials"
  description = "Redis credentials for ${local.project}-${local.environment}"

  tags = local.tags
}

resource "aws_secretsmanager_secret_version" "redis_credentials" {
  secret_id = aws_secretsmanager_secret.redis_credentials.id
  secret_string = jsonencode({
    host       = aws_elasticache_replication_group.main.primary_endpoint_address
    port       = 6379
    auth_token = random_password.redis_auth.result
    url        = "rediss://:${random_password.redis_auth.result}@${aws_elasticache_replication_group.main.primary_endpoint_address}:6379"
  })
}

# Kubernetes provider configuration
provider "kubernetes" {
  host                   = module.eks.cluster_endpoint
  cluster_ca_certificate = base64decode(module.eks.cluster_certificate_authority_data)

  exec {
    api_version = "client.authentication.k8s.io/v1beta1"
    command     = "aws"
    args        = ["eks", "get-token", "--cluster-name", module.eks.cluster_name]
  }
}

# Helm provider configuration
provider "helm" {
  kubernetes {
    host                   = module.eks.cluster_endpoint
    cluster_ca_certificate = base64decode(module.eks.cluster_certificate_authority_data)

    exec {
      api_version = "client.authentication.k8s.io/v1beta1"
      command     = "aws"
      args        = ["eks", "get-token", "--cluster-name", module.eks.cluster_name]
    }
  }
}

# Outputs
output "vpc_id" {
  description = "VPC ID"
  value       = module.vpc.vpc_id
}

output "eks_cluster_name" {
  description = "EKS cluster name"
  value       = module.eks.cluster_name
}

output "eks_cluster_endpoint" {
  description = "EKS cluster endpoint"
  value       = module.eks.cluster_endpoint
}

output "rds_endpoint" {
  description = "RDS endpoint"
  value       = module.rds.endpoint
}

output "rds_credentials_secret_arn" {
  description = "RDS credentials secret ARN"
  value       = module.rds.credentials_secret_arn
}

output "redis_endpoint" {
  description = "Redis endpoint"
  value       = aws_elasticache_replication_group.main.primary_endpoint_address
}

output "redis_credentials_secret_arn" {
  description = "Redis credentials secret ARN"
  value       = aws_secretsmanager_secret.redis_credentials.arn
}
