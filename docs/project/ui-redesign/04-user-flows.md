# VeloZ User Flows

## 1. Authentication Flows

### 1.1 Login Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              LOGIN FLOW                                      │
└─────────────────────────────────────────────────────────────────────────────┘

User visits app
      │
      ▼
┌─────────────┐     No      ┌─────────────┐
│ Has valid   │────────────►│ Show Login  │
│ access      │             │ Page        │
│ token?      │             └──────┬──────┘
└──────┬──────┘                    │
       │ Yes                       │
       ▼                           ▼
┌─────────────┐            ┌─────────────┐
│ Redirect to │            │ User enters │
│ Dashboard   │            │ credentials │
└─────────────┘            └──────┬──────┘
                                  │
                                  ▼
                           ┌─────────────┐
                           │ Validate    │
                           │ form inputs │
                           └──────┬──────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │                           │
                    ▼ Invalid                   ▼ Valid
             ┌─────────────┐            ┌─────────────┐
             │ Show field  │            │ POST /api/  │
             │ errors      │            │ auth/login  │
             └─────────────┘            └──────┬──────┘
                                               │
                                 ┌─────────────┴─────────────┐
                                 │                           │
                                 ▼ 401                       ▼ 200
                          ┌─────────────┐            ┌─────────────┐
                          │ Show error  │            │ Store tokens│
                          │ "Invalid    │            │ in memory   │
                          │ credentials"│            └──────┬──────┘
                          └─────────────┘                   │
                                                           ▼
                                                    ┌─────────────┐
                                                    │ Redirect to │
                                                    │ Dashboard   │
                                                    │ (or saved   │
                                                    │ URL)        │
                                                    └─────────────┘
```

### 1.2 Token Refresh Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           TOKEN REFRESH FLOW                                 │
└─────────────────────────────────────────────────────────────────────────────┘

API request
      │
      ▼
┌─────────────┐     401      ┌─────────────┐
│ Make API    │─────────────►│ Check if    │
│ request     │              │ refresh     │
└──────┬──────┘              │ token exists│
       │                     └──────┬──────┘
       │ Success                    │
       ▼                 ┌──────────┴──────────┐
┌─────────────┐          │                     │
│ Return      │          ▼ Yes                 ▼ No
│ response    │   ┌─────────────┐       ┌─────────────┐
└─────────────┘   │ POST /api/  │       │ Redirect to │
                  │ auth/refresh│       │ Login page  │
                  └──────┬──────┘       └─────────────┘
                         │
           ┌─────────────┴─────────────┐
           │                           │
           ▼ Success                   ▼ Failure
    ┌─────────────┐             ┌─────────────┐
    │ Update      │             │ Clear all   │
    │ access      │             │ tokens      │
    │ token       │             └──────┬──────┘
    └──────┬──────┘                    │
           │                           ▼
           ▼                    ┌─────────────┐
    ┌─────────────┐             │ Redirect to │
    │ Retry       │             │ Login page  │
    │ original    │             └─────────────┘
    │ request     │
    └─────────────┘
```

### 1.3 Logout Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              LOGOUT FLOW                                     │
└─────────────────────────────────────────────────────────────────────────────┘

User clicks logout
      │
      ▼
┌─────────────┐
│ POST /api/  │
│ auth/logout │
│ (revoke     │
│ refresh)    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Clear all   │
│ tokens from │
│ memory      │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Clear       │
│ application │
│ state       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Redirect to │
│ Login page  │
└─────────────┘
```

---

## 2. Trading Flows

### 2.1 Place Order Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            PLACE ORDER FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

User on Trading page
      │
      ▼
┌─────────────┐
│ Select      │
│ BUY/SELL    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Enter       │
│ quantity    │
│ and price   │
└──────┬──────┘
       │
       ▼
┌─────────────┐     Invalid    ┌─────────────┐
│ Validate    │───────────────►│ Show field  │
│ inputs      │                │ errors      │
└──────┬──────┘                └─────────────┘
       │ Valid
       ▼
┌─────────────┐     No         ┌─────────────┐
│ Confirmation│───────────────►│ POST /api/  │
│ enabled?    │                │ order       │
└──────┬──────┘                └──────┬──────┘
       │ Yes                          │
       ▼                              │
┌─────────────┐                       │
│ Show        │                       │
│ confirmation│                       │
│ modal       │                       │
└──────┬──────┘                       │
       │                              │
       ├─────────────┐                │
       │ Cancel      │                │
       ▼             ▼                │
┌─────────────┐ ┌─────────────┐       │
│ Close modal │ │ POST /api/  │◄──────┘
└─────────────┘ │ order       │
                └──────┬──────┘
                       │
         ┌─────────────┴─────────────┐
         │                           │
         ▼ Success                   ▼ Error
  ┌─────────────┐             ┌─────────────┐
  │ Show        │             │ Show error  │
  │ success     │             │ notification│
  │ notification│             │ (rate limit,│
  │             │             │ insufficient│
  │ Update      │             │ balance,    │
  │ order list  │             │ etc.)       │
  └─────────────┘             └─────────────┘
```

### 2.2 Cancel Order Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CANCEL ORDER FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

User clicks Cancel on order
      │
      ▼
┌─────────────┐
│ Show        │
│ confirmation│
│ dialog      │
│ "Cancel     │
│ order?"     │
└──────┬──────┘
       │
       ├─────────────┐
       │ No          │ Yes
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Close       │ │ POST /api/  │
│ dialog      │ │ cancel      │
└─────────────┘ └──────┬──────┘
                       │
         ┌─────────────┴─────────────┐
         │                           │
         ▼ Success                   ▼ Error
  ┌─────────────┐             ┌─────────────┐
  │ Show        │             │ Show error  │
  │ success     │             │ notification│
  │ notification│             │ (already    │
  │             │             │ filled,     │
  │ Update      │             │ not found)  │
  │ order list  │             └─────────────┘
  └─────────────┘
```

---

## 3. Strategy Flows

### 3.1 Create Strategy Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CREATE STRATEGY FLOW                                 │
└─────────────────────────────────────────────────────────────────────────────┘

User clicks "+ New Strategy"
      │
      ▼
┌─────────────┐
│ Open        │
│ strategy    │
│ form modal  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Select      │
│ strategy    │
│ type        │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Load        │
│ parameter   │
│ schema for  │
│ type        │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Fill in:    │
│ - Name      │
│ - Symbol    │
│ - Parameters│
│ - Risk      │
│   settings  │
└──────┬──────┘
       │
       ▼
┌─────────────┐     Invalid    ┌─────────────┐
│ Validate    │───────────────►│ Show field  │
│ all fields  │                │ errors      │
└──────┬──────┘                └─────────────┘
       │ Valid
       ▼
┌─────────────┐
│ POST /api/  │
│ strategies  │
└──────┬──────┘
       │
       ├─────────────┐
       │ Success     │ Error
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Close modal │ │ Show error  │
│ Show success│ │ message     │
│ Add to grid │ └─────────────┘
└─────────────┘
```

### 3.2 Start/Stop Strategy Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        START/STOP STRATEGY FLOW                              │
└─────────────────────────────────────────────────────────────────────────────┘

User clicks Start/Stop button
      │
      ▼
┌─────────────┐
│ Show        │
│ loading     │
│ state on    │
│ button      │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ POST /api/  │
│ strategies/ │
│ {id}/start  │
│ or /stop    │
└──────┬──────┘
       │
       ├─────────────┐
       │ Success     │ Error
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Update      │ │ Show error  │
│ strategy    │ │ notification│
│ status      │ │             │
│ badge       │ │ Reset       │
│             │ │ button      │
│ Show        │ │ state       │
│ notification│ └─────────────┘
└─────────────┘
```

### 3.3 Update Parameters Flow (Hot Update)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      UPDATE PARAMETERS FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

User clicks Edit on strategy
      │
      ▼
┌─────────────┐
│ Open        │
│ parameter   │
│ editor      │
│ modal       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Load        │
│ current     │
│ parameter   │
│ values      │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ User edits  │
│ parameters  │
│ (hot params │
│ marked)     │
└──────┬──────┘
       │
       ▼
┌─────────────┐     Invalid    ┌─────────────┐
│ Validate    │───────────────►│ Show field  │
│ changes     │                │ errors      │
└──────┬──────┘                └─────────────┘
       │ Valid
       ▼
┌─────────────┐
│ PUT /api/   │
│ strategies/ │
│ {id}/params │
└──────┬──────┘
       │
       ├─────────────┐
       │ Success     │ Error
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Close modal │ │ Show error  │
│ Show success│ │ message     │
│ Update card │ │             │
│ metrics     │ │ Keep modal  │
└─────────────┘ │ open        │
                └─────────────┘
```

---

## 4. Backtest Flows

### 4.1 Run Backtest Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           RUN BACKTEST FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

User on Backtest page
      │
      ▼
┌─────────────┐
│ Configure   │
│ backtest:   │
│ - Strategy  │
│ - Symbol    │
│ - Date range│
│ - Capital   │
│ - Parameters│
└──────┬──────┘
       │
       ▼
┌─────────────┐     Invalid    ┌─────────────┐
│ Validate    │───────────────►│ Show field  │
│ config      │                │ errors      │
└──────┬──────┘                └─────────────┘
       │ Valid
       ▼
┌─────────────┐
│ POST /api/  │
│ backtest/run│
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Show        │
│ progress    │
│ indicator   │
│ "Running    │
│ backtest..."|
└──────┬──────┘
       │
       ├─────────────┐
       │ Success     │ Error
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Add result  │ │ Show error  │
│ to list     │ │ notification│
│             │ │             │
│ Auto-select │ │ Hide        │
│ new result  │ │ progress    │
│             │ └─────────────┘
│ Show        │
│ details     │
│ panel       │
└─────────────┘
```

### 4.2 Compare Results Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        COMPARE RESULTS FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

User on Backtest page
      │
      ▼
┌─────────────┐
│ Check       │
│ multiple    │
│ results     │
│ (2-5)       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Click       │
│ "Compare"   │
│ button      │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Open        │
│ comparison  │
│ view        │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Display:    │
│ - Side-by-  │
│   side      │
│   metrics   │
│ - Overlaid  │
│   equity    │
│   curves    │
│ - Parameter │
│   diff      │
└─────────────┘
```

---

## 5. Admin Flows

### 5.1 View Audit Logs Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         VIEW AUDIT LOGS FLOW                                 │
└─────────────────────────────────────────────────────────────────────────────┘

Admin navigates to Audit page
      │
      ▼
┌─────────────┐     No         ┌─────────────┐
│ Check admin │───────────────►│ Show        │
│ permission  │                │ "Access     │
└──────┬──────┘                │ Denied"     │
       │ Yes                   └─────────────┘
       ▼
┌─────────────┐
│ GET /api/   │
│ audit/logs  │
│ (default    │
│ filters)    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Display     │
│ logs in     │
│ table       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ User can:   │
│ - Filter by │
│   type      │
│ - Filter by │
│   user      │
│ - Filter by │
│   date      │
│ - Paginate  │
│ - Export    │
└─────────────┘
```

### 5.2 Export Audit Logs Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        EXPORT AUDIT LOGS FLOW                                │
└─────────────────────────────────────────────────────────────────────────────┘

Admin clicks "Export CSV"
      │
      ▼
┌─────────────┐
│ Show export │
│ options     │
│ modal:      │
│ - Date range│
│ - Log types │
│ - Format    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Generate    │
│ export      │
│ (client-    │
│ side)       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Download    │
│ file        │
│ (CSV/JSON)  │
└─────────────┘
```

---

## 6. Real-Time Update Flows

### 6.1 SSE Connection Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SSE CONNECTION FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

App initializes
      │
      ▼
┌─────────────┐
│ Create      │
│ EventSource │
│ connection  │
│ to /api/    │
│ stream      │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Set status  │
│ "Connecting"│
└──────┬──────┘
       │
       ├─────────────┐
       │ onopen      │ onerror
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Set status  │ │ Set status  │
│ "Connected" │ │ "Disconnected"
│             │ │             │
│ Subscribe   │ │ Schedule    │
│ to events   │ │ reconnect   │
└──────┬──────┘ │ (exponential│
       │        │ backoff)    │
       ▼        └─────────────┘
┌─────────────┐
│ Handle      │
│ events:     │
│ - depth     │
│ - position  │
│ - order_    │
│   update    │
│ - fill      │
│ - account   │
│ - error     │
└─────────────┘
```

### 6.2 Order Book Update Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       ORDER BOOK UPDATE FLOW                                 │
└─────────────────────────────────────────────────────────────────────────────┘

SSE receives "depth" event
      │
      ▼
┌─────────────┐
│ Parse       │
│ event data  │
└──────┬──────┘
       │
       ▼
┌─────────────┐     Yes        ┌─────────────┐
│ Throttle    │───────────────►│ Queue       │
│ active?     │                │ update      │
└──────┬──────┘                └─────────────┘
       │ No
       ▼
┌─────────────┐
│ Check if    │
│ snapshot    │
│ or delta    │
└──────┬──────┘
       │
       ├─────────────┐
       │ Snapshot    │ Delta
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Replace     │ │ Update      │
│ entire      │ │ individual  │
│ book        │ │ levels      │
└──────┬──────┘ └──────┬──────┘
       │               │
       └───────┬───────┘
               │
               ▼
        ┌─────────────┐
        │ Recalculate │
        │ cumulative  │
        │ totals      │
        └──────┬──────┘
               │
               ▼
        ┌─────────────┐
        │ Trigger     │
        │ re-render   │
        │ with        │
        │ animations  │
        └─────────────┘
```

---

## 7. Error Handling Flows

### 7.1 API Error Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          API ERROR FLOW                                      │
└─────────────────────────────────────────────────────────────────────────────┘

API request fails
      │
      ▼
┌─────────────┐
│ Check       │
│ error       │
│ status      │
└──────┬──────┘
       │
       ├─────────────┬─────────────┬─────────────┐
       │ 401         │ 403         │ 429         │ Other
       ▼             ▼             ▼             ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│ Try token   │ │ Show        │ │ Show rate   │ │ Show        │
│ refresh     │ │ "Access     │ │ limit       │ │ generic     │
│ (see 1.2)   │ │ Denied"     │ │ notification│ │ error       │
└─────────────┘ │ notification│ │             │ │ notification│
                │             │ │ Disable     │ │             │
                │ Log error   │ │ actions     │ │ Log error   │
                │ for admin   │ │ temporarily │ │ for admin   │
                └─────────────┘ └─────────────┘ └─────────────┘
```

### 7.2 Connection Lost Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        CONNECTION LOST FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

SSE connection lost
      │
      ▼
┌─────────────┐
│ Update      │
│ status to   │
│ "Disconnected"
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Show        │
│ connection  │
│ warning     │
│ banner      │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Start       │
│ reconnect   │
│ with        │
│ backoff:    │
│ 1s, 2s, 4s, │
│ 8s, 16s,    │
│ max 30s     │
└──────┬──────┘
       │
       ├─────────────┐
       │ Success     │ Max retries
       ▼             ▼
┌─────────────┐ ┌─────────────┐
│ Update      │ │ Show        │
│ status to   │ │ "Connection │
│ "Connected" │ │ failed"     │
│             │ │ error       │
│ Hide        │ │             │
│ warning     │ │ Offer       │
│ banner      │ │ manual      │
│             │ │ retry       │
│ Fetch       │ └─────────────┘
│ latest      │
│ data        │
└─────────────┘
```

---

## 8. Theme Toggle Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          THEME TOGGLE FLOW                                   │
└─────────────────────────────────────────────────────────────────────────────┘

User clicks theme toggle
      │
      ▼
┌─────────────┐
│ Get current │
│ theme       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Toggle to   │
│ opposite    │
│ theme       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Update      │
│ Zustand     │
│ store       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Save to     │
│ localStorage│
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Update      │
│ Ant Design  │
│ ConfigProvider
│ theme       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Apply       │
│ data-theme  │
│ attribute   │
│ to document │
└─────────────┘
```

---

## 9. Navigation Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          NAVIGATION FLOW                                     │
└─────────────────────────────────────────────────────────────────────────────┘

                              ┌─────────────┐
                              │   Login     │
                              └──────┬──────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │  Dashboard  │
                              └──────┬──────┘
                                     │
           ┌─────────────────────────┼─────────────────────────┐
           │                         │                         │
           ▼                         ▼                         ▼
    ┌─────────────┐           ┌─────────────┐           ┌─────────────┐
    │   Trading   │           │  Positions  │           │  Strategies │
    │             │           │             │           │             │
    │ - Order Book│           │ - Position  │           │ - Strategy  │
    │ - Order Form│           │   Table     │           │   Grid      │
    │ - Order List│           │ - PnL       │           │ - Create    │
    │ - Trades    │           │   Summary   │           │ - Edit      │
    └─────────────┘           │ - PnL Chart │           │ - Metrics   │
                              └─────────────┘           └─────────────┘
           │                         │                         │
           │                         │                         │
           │                         ▼                         │
           │                  ┌─────────────┐                  │
           │                  │   Backtest  │                  │
           │                  │             │                  │
           │                  │ - Config    │                  │
           │                  │ - Results   │                  │
           │                  │ - Charts    │                  │
           │                  │ - Compare   │                  │
           │                  └─────────────┘                  │
           │                         │                         │
           └─────────────────────────┼─────────────────────────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │   Account   │
                              │             │
                              │ - Balances  │
                              │ - API Keys  │
                              │ - Settings  │
                              └──────┬──────┘
                                     │
                                     │ (Admin only)
                                     ▼
                              ┌─────────────┐
                              │ Audit Logs  │
                              │             │
                              │ - Log Table │
                              │ - Filters   │
                              │ - Stats     │
                              │ - Export    │
                              └─────────────┘
```
