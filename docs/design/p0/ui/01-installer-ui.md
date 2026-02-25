# P0 Feature 1: One-Click Installer UI

## Overview

The installer provides a seamless, guided experience for users to install VeloZ on their system. It should feel professional, trustworthy, and simple enough for non-technical users.

---

## User Flow

```
[Download] -> [Welcome] -> [License] -> [Install Location] -> [Installing] -> [Complete]
```

---

## Screen 1: Welcome Screen

### Wireframe
```
+------------------------------------------------------------------+
|                                                                  |
|                         [VeloZ Logo]                             |
|                                                                  |
|                    Welcome to VeloZ                              |
|                                                                  |
|            Professional Crypto Trading Platform                  |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  VeloZ is a quantitative trading framework that      |     |
|     |  helps you automate your crypto trading strategies.  |     |
|     |                                                      |     |
|     |  This installer will guide you through the setup     |     |
|     |  process in just a few minutes.                      |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     System Requirements:                                         |
|     [Check] macOS 12+ / Windows 10+ / Ubuntu 20.04+             |
|     [Check] 4GB RAM minimum                                      |
|     [Check] 2GB disk space                                       |
|                                                                  |
|                                                                  |
|                                      [Get Started ->]            |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Window Size**: 600px x 500px (fixed)
- **Logo**: 120px x 120px, centered
- **Title**: text-3xl (24px), font-semibold, text-text
- **Subtitle**: text-lg (16px), text-text-muted
- **Body Text**: text-base (13px), text-text
- **System Checks**:
  - Green checkmark for passed requirements
  - Red X with warning for failed requirements
- **Button**: Primary button, right-aligned

### Platform-Specific Considerations

#### macOS
- Native `.dmg` installer with drag-to-Applications
- Notarization badge displayed
- Gatekeeper compatibility message if needed

#### Windows
- Standard Windows installer aesthetic
- UAC elevation prompt warning
- Windows Defender SmartScreen guidance

#### Linux
- AppImage or .deb/.rpm options
- Terminal fallback instructions link

---

## Screen 2: License Agreement

### Wireframe
```
+------------------------------------------------------------------+
|                                                                  |
|  [<- Back]                                        Step 1 of 4    |
|                                                                  |
|                    License Agreement                             |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  MIT License                                         |     |
|     |                                                      |     |
|     |  Copyright (c) 2026 VeloZ                           |     |
|     |                                                      |     |
|     |  Permission is hereby granted, free of charge,      |     |
|     |  to any person obtaining a copy of this software    |     |
|     |  and associated documentation files (the            |     |
|     |  "Software"), to deal in the Software without       |     |
|     |  restriction, including without limitation the      |     |
|     |  rights to use, copy, modify, merge, publish,       |     |
|     |  distribute, sublicense, and/or sell copies of      |     |
|     |  the Software...                                    |     |
|     |                                                      |  v  |
|     +------------------------------------------------------+     |
|                                                                  |
|     [ ] I have read and agree to the license terms              |
|                                                                  |
|                                                                  |
|     [<- Back]                            [Continue ->]           |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **License Text Area**: Scrollable, 400px height, border, rounded-lg
- **Checkbox**: Required before Continue button enables
- **Continue Button**: Disabled state until checkbox checked

---

## Screen 3: Installation Location

### Wireframe
```
+------------------------------------------------------------------+
|                                                                  |
|  [<- Back]                                        Step 2 of 4    |
|                                                                  |
|                  Choose Install Location                         |
|                                                                  |
|     Install VeloZ to:                                            |
|                                                                  |
|     +--------------------------------------------------+ [Browse]|
|     | /Applications/VeloZ                              |         |
|     +--------------------------------------------------+         |
|                                                                  |
|     Space required: 1.8 GB                                       |
|     Space available: 245.3 GB                                    |
|                                                                  |
|     +------------------------------------------------------+     |
|     |  [i] Recommended: Use the default location for       |     |
|     |      automatic updates and easier configuration.     |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Additional Options:                                          |
|     [ ] Create desktop shortcut                                  |
|     [ ] Add to system PATH (for CLI access)                     |
|     [x] Launch VeloZ after installation                         |
|                                                                  |
|                                                                  |
|     [<- Back]                              [Install ->]          |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Path Input**: Full-width input with Browse button
- **Space Indicators**:
  - Green text if sufficient space
  - Red text with warning if insufficient
- **Info Box**: background-secondary, rounded-lg, info icon
- **Checkboxes**: Standard checkbox styling

---

## Screen 4: Installation Progress

### Wireframe
```
+------------------------------------------------------------------+
|                                                                  |
|                                                  Step 3 of 4     |
|                                                                  |
|                      Installing VeloZ                            |
|                                                                  |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [========================================>         ] |     |
|     |                                            78%       |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Current task: Installing trading engine...                   |
|                                                                  |
|     +------------------------------------------------------+     |
|     |  [Check] Downloading core components        1.2 GB   |     |
|     |  [Check] Extracting files                            |     |
|     |  [Spin]  Installing trading engine                   |     |
|     |  [ ]     Configuring system services                 |     |
|     |  [ ]     Setting up default configuration            |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Estimated time remaining: 2 minutes                          |
|                                                                  |
|                                                                  |
|                                              [Cancel]            |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Progress Bar**:
  - Height: 8px
  - Background: background-tertiary
  - Fill: installer-progress (#2563eb)
  - Rounded-full ends
  - Animated fill transition
- **Task List**:
  - Completed: Green checkmark
  - In Progress: Spinning loader
  - Pending: Gray circle
- **Cancel Button**: Secondary button, triggers confirmation dialog

### Cancel Confirmation Dialog
```
+------------------------------------------+
|  Cancel Installation?                    |
+------------------------------------------+
|                                          |
|  Are you sure you want to cancel?        |
|  Any downloaded files will be removed.   |
|                                          |
|        [Continue Installing]  [Cancel]   |
+------------------------------------------+
```

---

## Screen 5: Installation Complete

### Wireframe (Success)
```
+------------------------------------------------------------------+
|                                                                  |
|                                                  Step 4 of 4     |
|                                                                  |
|                    [Large Green Checkmark]                       |
|                                                                  |
|                  Installation Complete!                          |
|                                                                  |
|     VeloZ has been successfully installed on your system.        |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  What's Next?                                        |     |
|     |                                                      |     |
|     |  1. Configure your exchange API keys                 |     |
|     |  2. Set up your risk parameters                      |     |
|     |  3. Explore the strategy marketplace                 |     |
|     |  4. Start with simulated trading                     |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [ ] Open Getting Started guide                              |
|                                                                  |
|                                                                  |
|                                        [Launch VeloZ ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

### Wireframe (Error)
```
+------------------------------------------------------------------+
|                                                                  |
|                                                  Step 4 of 4     |
|                                                                  |
|                      [Large Red X]                               |
|                                                                  |
|                  Installation Failed                             |
|                                                                  |
|     An error occurred during installation.                       |
|                                                                  |
|     +------------------------------------------------------+     |
|     |  Error Details:                                      |     |
|     |                                                      |     |
|     |  Code: E_DISK_FULL                                   |     |
|     |  Message: Insufficient disk space to complete        |     |
|     |           installation. Please free up at least      |     |
|     |           500 MB and try again.                      |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Need help? Visit our troubleshooting guide:                  |
|     [Link] docs.veloz.io/troubleshooting                        |
|                                                                  |
|                                                                  |
|     [<- Try Again]                              [Close]          |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Success Icon**: 80px, success color (#059669)
- **Error Icon**: 80px, danger color (#dc2626)
- **Error Box**: danger-light background, danger border, rounded-lg
- **Help Link**: info color, underlined

---

## Responsive Behavior

### Desktop (>= 1024px)
- Fixed window size: 600px x 500px
- Centered on screen
- Drop shadow for floating effect

### Tablet (768px - 1023px)
- Full-width with 24px padding
- Maximum width: 600px
- Centered content

### Mobile (< 768px)
- Full-screen layout
- Stacked buttons
- Larger touch targets (48px minimum)
- Simplified progress display

---

## Accessibility

### Keyboard Navigation
- Tab through all interactive elements
- Enter/Space to activate buttons
- Escape to cancel/close dialogs

### Screen Reader
- Progress percentage announced on change
- Task completion status announced
- Error messages read automatically

### Visual
- High contrast mode support
- Focus indicators on all interactive elements
- No reliance on color alone for status

---

## User Testing Recommendations

### Test Scenarios
1. **Happy Path**: Complete installation with default settings
2. **Custom Location**: Install to non-default directory
3. **Insufficient Space**: Attempt install with low disk space
4. **Cancel Mid-Install**: Cancel and verify cleanup
5. **Network Failure**: Simulate download interruption
6. **Permission Denied**: Test without admin privileges

### Metrics to Track
- Time to complete installation
- Drop-off rate at each step
- Error frequency by type
- Support ticket volume post-install

### A/B Testing Ideas
- Progress bar vs. step indicators
- Checkbox placement for license agreement
- "What's Next" content variations
