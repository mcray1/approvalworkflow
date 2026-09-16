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

## Technical outline

- Frontend and API: React/Vinext application running as a Cloudflare Worker
- Structured persistence: D1/SQLite with generated Drizzle migrations
- File persistence: R2 object storage
- Identity: OpenAI authenticated-user headers supplied by the private Site
- Authorization: Server-side role, permission, requester, and current-assignment checks
- Source project: `approval-flow`

Core data includes request types, workflows, roles, people, approval requests, approval steps, activity, notifications, and attachment metadata.

## Administration checklist

Before broader rollout:

1. Review every request type and disable unused defaults.
2. Confirm each workflow's role assignment and response window.
3. Assign the correct role to every approver using their exact sign-in email.
4. Submit a test request for each workflow and complete its full approval route.
5. Verify that unauthorized users cannot see or download another request's files.
6. Decide who should have access to the private Site before expanding the audience.

## Operational notes

- Database changes are delivered through ordered migration files. Applied migrations must not be edited; future schema changes should add a new migration.
- Workflow deadlines are elapsed-hour targets. They do not currently pause for weekends or holidays.
- Overdue requests are highlighted in the inbox. Automated email delivery and scheduled escalation are not yet included.
- If an attachment upload fails after the request is created, the request remains saved and the user sees a recoverable error.

## Recommended next iteration

1. Add scheduled escalations for overdue approvals.
2. Add email or Slack delivery for assignments and reminders.
3. Add reusable custom form fields per request type.
4. Add reporting for cycle time, bottlenecks, and overdue volume.
5. Add reassignment and delegation for approver absences.

## Release verification

- Production build completes successfully.
- The release includes the D1 migration for response deadlines and attachment metadata.
- The Site configuration declares both the database and private file-storage bindings.
