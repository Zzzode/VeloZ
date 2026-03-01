import { Menu, Bell, User, LogOut } from 'lucide-react';
import { Menu as HeadlessMenu, MenuButton, MenuItems, MenuItem, Transition } from '@headlessui/react';
import { Fragment } from 'react';
import { Link } from 'react-router-dom';
import { TradingModeBadge } from '@/features/security-education/components/TradingModeBadge';
import { useSecurityEducationStore } from '@/features/security-education/store';

interface HeaderProps {
  onMenuClick?: () => void;
  showMenuButton?: boolean;
}

export function Header({ onMenuClick, showMenuButton = true }: HeaderProps) {
  const { tradingMode } = useSecurityEducationStore();

  return (
    <header className="h-14 bg-background border-b border-border px-4 flex items-center justify-between sticky top-0 z-40">
      {/* Left side */}
      <div className="flex items-center gap-4">
        {showMenuButton && (
          <button
            type="button"
            className="p-2 rounded-md text-text-muted hover:text-text hover:bg-gray-100 lg:hidden"
            onClick={onMenuClick}
          >
            <Menu className="h-5 w-5" />
          </button>
        )}
        <Link to="/" className="flex items-center gap-2">
          <span className="text-xl font-bold text-primary">VeloZ</span>
        </Link>
      </div>

      {/* Right side */}
      <div className="flex items-center gap-2">
        {/* Trading Mode Badge */}
        <Link to="/security">
          <TradingModeBadge mode={tradingMode} size="sm" />
        </Link>

        {/* Notifications */}
        <button
          type="button"
          className="p-2 rounded-md text-text-muted hover:text-text hover:bg-gray-100 relative"
        >
          <Bell className="h-5 w-5" />
          <span className="absolute top-1.5 right-1.5 h-2 w-2 bg-danger rounded-full" />
        </button>

        {/* User menu */}
        <HeadlessMenu as="div" className="relative">
          <MenuButton className="flex items-center gap-2 p-2 rounded-md text-text-muted hover:text-text hover:bg-gray-100">
            <User className="h-5 w-5" />
          </MenuButton>

          <Transition
            as={Fragment}
            enter="transition ease-out duration-100"
            enterFrom="transform opacity-0 scale-95"
            enterTo="transform opacity-100 scale-100"
            leave="transition ease-in duration-75"
            leaveFrom="transform opacity-100 scale-100"
            leaveTo="transform opacity-0 scale-95"
          >
            <MenuItems className="absolute right-0 mt-2 w-48 origin-top-right rounded-md bg-background shadow-lg ring-1 ring-black ring-opacity-5 focus:outline-none">
              <div className="py-1">
                <MenuItem>
                  {({ focus }) => (
                    <Link
                      to="/settings"
                      className={`${
                        focus ? 'bg-gray-100' : ''
                      } flex items-center gap-2 px-4 py-2 text-sm text-text`}
                    >
                      <User className="h-4 w-4" />
                      Profile
                    </Link>
                  )}
                </MenuItem>
                <MenuItem>
                  {({ focus }) => (
                    <button
                      type="button"
                      className={`${
                        focus ? 'bg-gray-100' : ''
                      } flex items-center gap-2 w-full px-4 py-2 text-sm text-danger`}
                    >
                      <LogOut className="h-4 w-4" />
                      Sign out
                    </button>
                  )}
                </MenuItem>
              </div>
            </MenuItems>
          </Transition>
        </HeadlessMenu>
      </div>
    </header>
  );
}
