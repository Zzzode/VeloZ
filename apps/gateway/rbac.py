#!/usr/bin/env python3
"""Role-Based Access Control (RBAC) module for VeloZ gateway.

This module provides user role management and integrates with the existing
permission system defined in gateway.py.
"""

import threading
import time
import logging
from dataclasses import dataclass, field
from typing import Optional

# Configure audit logger
audit_logger = logging.getLogger("veloz.audit")


@dataclass
class UserRole:
    """Represents a user's role assignment."""
    user_id: str
    roles: list = field(default_factory=list)
    created_at: int = field(default_factory=lambda: int(time.time()))
    updated_at: int = field(default_factory=lambda: int(time.time()))
    created_by: Optional[str] = None


class RoleManager:
    """Manages user role assignments for RBAC.

    This class stores user-to-role mappings and provides methods for
    role assignment, removal, and querying.
    """

    # Default roles for new users without explicit assignment
    DEFAULT_ROLES = ["viewer"]

    def __init__(self):
        self._mu = threading.Lock()
        # user_id -> UserRole
        self._user_roles: dict[str, UserRole] = {}

    def assign_role(self, user_id: str, role: str, assigned_by: Optional[str] = None) -> bool:
        """Assign a role to a user.

        Args:
            user_id: The user to assign the role to
            role: The role to assign (e.g., 'viewer', 'trader', 'admin')
            assigned_by: The user making the assignment (for audit)

        Returns:
            True if role was assigned, False if user already had the role
        """
        with self._mu:
            if user_id not in self._user_roles:
                self._user_roles[user_id] = UserRole(
                    user_id=user_id,
                    roles=[],
                    created_by=assigned_by,
                )

            user_role = self._user_roles[user_id]
            if role in user_role.roles:
                return False

            user_role.roles.append(role)
            user_role.updated_at = int(time.time())

        audit_logger.info(
            f"Role assigned: user={user_id}, role={role}, by={assigned_by or 'system'}"
        )
        return True

    def remove_role(self, user_id: str, role: str, removed_by: Optional[str] = None) -> bool:
        """Remove a role from a user.

        Args:
            user_id: The user to remove the role from
            role: The role to remove
            removed_by: The user making the removal (for audit)

        Returns:
            True if role was removed, False if user didn't have the role
        """
        with self._mu:
            if user_id not in self._user_roles:
                return False

            user_role = self._user_roles[user_id]
            if role not in user_role.roles:
                return False

            user_role.roles.remove(role)
            user_role.updated_at = int(time.time())

        audit_logger.info(
            f"Role removed: user={user_id}, role={role}, by={removed_by or 'system'}"
        )
        return True

    def set_roles(self, user_id: str, roles: list, set_by: Optional[str] = None) -> None:
        """Set all roles for a user, replacing existing roles.

        Args:
            user_id: The user to set roles for
            roles: List of roles to assign
            set_by: The user making the change (for audit)
        """
        with self._mu:
            if user_id not in self._user_roles:
                self._user_roles[user_id] = UserRole(
                    user_id=user_id,
                    roles=list(roles),
                    created_by=set_by,
                )
            else:
                user_role = self._user_roles[user_id]
                user_role.roles = list(roles)
                user_role.updated_at = int(time.time())

        audit_logger.info(
            f"Roles set: user={user_id}, roles={roles}, by={set_by or 'system'}"
        )

    def get_roles(self, user_id: str) -> list:
        """Get all roles for a user.

        Args:
            user_id: The user to get roles for

        Returns:
            List of role names, or DEFAULT_ROLES if user has no explicit assignment
        """
        with self._mu:
            if user_id not in self._user_roles:
                return list(self.DEFAULT_ROLES)
            return list(self._user_roles[user_id].roles)

    def has_role(self, user_id: str, role: str) -> bool:
        """Check if a user has a specific role.

        Args:
            user_id: The user to check
            role: The role to check for

        Returns:
            True if user has the role
        """
        roles = self.get_roles(user_id)
        return role in roles

    def get_user_role_info(self, user_id: str) -> Optional[dict]:
        """Get detailed role information for a user.

        Args:
            user_id: The user to get info for

        Returns:
            Dict with role info, or None if user has no explicit assignment
        """
        with self._mu:
            if user_id not in self._user_roles:
                return None
            user_role = self._user_roles[user_id]
            return {
                "user_id": user_role.user_id,
                "roles": list(user_role.roles),
                "created_at": user_role.created_at,
                "updated_at": user_role.updated_at,
                "created_by": user_role.created_by,
            }

    def list_users_with_role(self, role: str) -> list:
        """List all users that have a specific role.

        Args:
            role: The role to search for

        Returns:
            List of user IDs with the role
        """
        with self._mu:
            return [
                user_id
                for user_id, user_role in self._user_roles.items()
                if role in user_role.roles
            ]

    def list_all_user_roles(self) -> list:
        """List all user role assignments.

        Returns:
            List of dicts with user role info
        """
        with self._mu:
            return [
                {
                    "user_id": user_role.user_id,
                    "roles": list(user_role.roles),
                    "created_at": user_role.created_at,
                    "updated_at": user_role.updated_at,
                }
                for user_role in self._user_roles.values()
            ]

    def delete_user(self, user_id: str, deleted_by: Optional[str] = None) -> bool:
        """Delete all role assignments for a user.

        Args:
            user_id: The user to delete
            deleted_by: The user making the deletion (for audit)

        Returns:
            True if user was deleted, False if user didn't exist
        """
        with self._mu:
            if user_id not in self._user_roles:
                return False
            del self._user_roles[user_id]

        audit_logger.info(
            f"User roles deleted: user={user_id}, by={deleted_by or 'system'}"
        )
        return True

    def get_metrics(self) -> dict:
        """Get metrics about role assignments.

        Returns:
            Dict with role assignment metrics
        """
        with self._mu:
            role_counts: dict[str, int] = {}
            for user_role in self._user_roles.values():
                for role in user_role.roles:
                    role_counts[role] = role_counts.get(role, 0) + 1

            return {
                "total_users": len(self._user_roles),
                "role_counts": role_counts,
            }


# Global role manager instance
role_manager = RoleManager()


def get_user_permissions(user_id: str, role_permissions: dict) -> list:
    """Get expanded permissions for a user based on their roles.

    This function integrates with the existing permission system by
    returning the user's roles, which can then be expanded by the
    PermissionManager.

    Args:
        user_id: The user to get permissions for
        role_permissions: The ROLE_PERMISSIONS dict from gateway.py

    Returns:
        List of roles/permissions for the user
    """
    roles = role_manager.get_roles(user_id)
    return roles
