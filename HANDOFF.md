# ClearPath Approvals — Project Handoff

## Live application

- Production: https://clearpath-approvals.pcbossbn.chatgpt.site
- Access: Private to the configured Site audience
- Product: Internal request and multi-step approval workflow

## What is included

- Approval inbox with search, status filters, request details, and step-by-step routing
- Request submission with optional supporting files
- Approve or return decisions with notes and an activity trail
- Role-based request visibility and server-side decision authorization
- In-app notifications for new assignments and completed decisions
- Administrator configuration for request types, approval hierarchies, roles, and people
- Per-step response windows, live due-time labels, and overdue highlighting

## Roles and permissions

| Role | Default responsibility |
| --- | --- |
| Administrator | Configure the app, view all requests, submit, and approve |
| Approver | Review requests assigned to their role and record decisions |
| Requester | Submit requests and follow their own requests |

Administrators can create additional roles, choose granular permissions, and assign roles to people by sign-in email.

## Approval behavior

1. A requester selects an active request type and submits the required details.
2. ClearPath copies the request type's active workflow into durable approval steps.
3. The first step becomes pending and receives its configured response deadline.
4. Approval activates the next step and starts that step's response clock.
5. The last approval completes the request; a return sends it back to the requester.
6. Notifications are created for the next responsible role or the requester.

Existing requests keep the route that was active when they were submitted. Later workflow edits affect new requests only.

## Supporting files

- Accepted formats: PDF, JPEG, PNG, Word, and Excel
- Limit: 3 files per submission, 10 MB per file
- File bytes are stored in private object storage; searchable metadata stays in the application database.
- Downloads are authorized on the server. Administrators, the requester, and the currently assigned approver can access a request's attachments.

## ABC app integration

ClearPath will connect to the ABC app as its source of school reference data. Request forms and approval routing will use the records the ABC app already maintains instead of asking requesters to type them in.

### Data pulled from the ABC app

| Record | Fields used by ClearPath | Used for |
| --- | --- | --- |
| Students | Student ID, full name, grade level, section | Request forms that concern a specific student; request search and details |
| Teachers | Teacher ID, full name, sign-in email, department, advisory section | Request forms, matching approvers to people, routing to an adviser or department head |
| Subjects | Subject code, subject name, grade level | Request forms that concern a subject or class |
| Other records | To be confirmed with the ABC app owner (for example sections, school year, departments) | Additional form fields and routing rules |

ClearPath reads from the ABC app only. It never writes student, teacher, or subject records back.

### How the connection works

1. An administrator enters the ABC app base URL and stores the access credential as a Worker secret. The credential is never stored in D1 or shown in the UI.
2. The Worker calls the ABC app from the server. The browser never talks to the ABC app directly, so the credential stays private.
3. A sync job copies the current students, teachers, and subjects into read-only reference tables in D1 and records the sync time.
4. Request forms offer these records through searchable lookup fields (for example, pick a student by name or ID) instead of free-text entry.
5. A request stores the ABC record ID plus the display name at submission time. Later changes in the ABC app do not rewrite an existing request's history.
6. Approval steps can be routed by the matched record, for example to the teacher's department head.

### Sync and failure handling

- Sync runs on a schedule and can also be triggered manually by an administrator.
- If the ABC app is unreachable, ClearPath keeps serving the last synced records and shows the last successful sync time.
- Records that disappear from the ABC app are marked inactive rather than deleted so existing requests keep their references.
- Lookup fields only offer active records.

### Open items to confirm with the ABC app owner

- API base URL, authentication method, and rate limits
- Exact field names and which records beyond students, teachers, and subjects are needed
- Whether teacher sign-in emails in the ABC app match the emails used for ClearPath roles
- How often the data changes and the acceptable sync interval

## Technical outline

- Frontend and API: React/Vinext application running as a Cloudflare Worker
- Structured persistence: D1/SQLite with generated Drizzle migrations
- File persistence: R2 object storage
- Identity: OpenAI authenticated-user headers supplied by the private Site
- Authorization: Server-side role, permission, requester, and current-assignment checks
- Reference data: ABC app records (students, teachers, subjects) read through a server-side client and cached in D1
- Source project: `approval-flow`

Core data includes request types, workflows, roles, people, approval requests, approval steps, activity, notifications, attachment metadata, and synced ABC reference records (students, teachers, and subjects).

## Administration checklist

Before broader rollout:

1. Review every request type and disable unused defaults.
2. Confirm each workflow's role assignment and response window.
3. Assign the correct role to every approver using their exact sign-in email.
4. Submit a test request for each workflow and complete its full approval route.
5. Verify that unauthorized users cannot see or download another request's files.
6. Decide who should have access to the private Site before expanding the audience.
7. Enter the ABC app connection, run a full sync, and spot-check one student, one teacher, and one subject against the ABC app.
8. Confirm that each request type's lookup fields point to the correct ABC record type.

## Operational notes

- Database changes are delivered through ordered migration files. Applied migrations must not be edited; future schema changes should add a new migration.
- Workflow deadlines are elapsed-hour targets. They do not currently pause for weekends or holidays.
- Overdue requests are highlighted in the inbox. Automated email delivery and scheduled escalation are not yet included.
- If an attachment upload fails after the request is created, the request remains saved and the user sees a recoverable error.
- ABC reference data is read-only in ClearPath. Corrections must be made in the ABC app and are picked up on the next sync.
- The ABC app credential lives in a Worker secret. Rotating it means updating the secret; no code change is needed.

## Recommended next iteration

1. Connect to the ABC app for students, teachers, and subjects (see ABC app integration above).
2. Add scheduled escalations for overdue approvals.
3. Add email or Slack delivery for assignments and reminders.
4. Add reusable custom form fields per request type, including ABC lookup fields.
5. Add reporting for cycle time, bottlenecks, and overdue volume.
6. Add reassignment and delegation for approver absences.

## Release verification

- Production build completes successfully.
- The release includes the D1 migration for response deadlines and attachment metadata.
- The Site configuration declares both the database and private file-storage bindings.
- The release that ships the ABC app integration must add the migration for the reference tables and declare the ABC base URL variable and credential secret.
