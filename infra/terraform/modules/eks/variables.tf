# EKS Module Variables

variable "project" {
  description = "Project name"
  type        = string
  default     = "veloz"
}

variable "environment" {
  description = "Environment name"
  type        = string
}

variable "vpc_id" {
  description = "VPC ID"
  type        = string
}

variable "subnet_ids" {
  description = "Subnet IDs for EKS cluster"
  type        = list(string)
}

variable "node_subnet_ids" {
  description = "Subnet IDs for EKS node groups"
  type        = list(string)
}

variable "cluster_version" {
  description = "Kubernetes version"
  type        = string
  default     = "1.29"
}

variable "endpoint_private_access" {
  description = "Enable private API endpoint"
  type        = bool
  default     = true
}

variable "endpoint_public_access" {
  description = "Enable public API endpoint"
  type        = bool
  default     = true
}

variable "public_access_cidrs" {
  description = "CIDR blocks allowed to access public API endpoint"
  type        = list(string)
  default     = ["0.0.0.0/0"]
}

variable "enabled_cluster_log_types" {
  description = "List of control plane log types to enable"
  type        = list(string)
  default     = ["api", "audit", "authenticator", "controllerManager", "scheduler"]
}

variable "cluster_log_retention_days" {
  description = "Retention period for cluster logs"
  type        = number
  default     = 30
}

variable "kms_key_arn" {
  description = "KMS key ARN for secrets encryption (creates new key if empty)"
  type        = string
  default     = ""
}

variable "node_groups" {
  description = "Map of node group configurations"
  type = map(object({
    instance_types             = list(string)
    capacity_type              = string
    disk_size                  = number
    desired_size               = number
    min_size                   = number
    max_size                   = number
    max_unavailable_percentage = number
    labels                     = map(string)
    taints = list(object({
      key    = string
      value  = string
      effect = string
    }))
  }))
  default = {
    general = {
      instance_types             = ["t3.medium"]
      capacity_type              = "ON_DEMAND"
      disk_size                  = 50
      desired_size               = 2
      min_size                   = 1
      max_size                   = 5
      max_unavailable_percentage = 50
      labels                     = {}
      taints                     = []
    }
  }
}

variable "vpc_cni_version" {
  description = "VPC CNI addon version"
  type        = string
  default     = "v1.16.0-eksbuild.1"
}

variable "coredns_version" {
  description = "CoreDNS addon version"
  type        = string
  default     = "v1.11.1-eksbuild.6"
}

variable "kube_proxy_version" {
  description = "kube-proxy addon version"
  type        = string
  default     = "v1.29.0-eksbuild.1"
}

variable "enable_ebs_csi_driver" {
  description = "Enable EBS CSI driver addon"
  type        = bool
  default     = true
}

variable "ebs_csi_driver_version" {
  description = "EBS CSI driver addon version"
  type        = string
  default     = "v1.27.0-eksbuild.1"
}

variable "tags" {
  description = "Tags to apply to resources"
  type        = map(string)
  default     = {}
}
