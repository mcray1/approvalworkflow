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

## AttendTrack (ABC) integration

ClearPath will connect to AttendTrack, the school's attendance and gradesheet system, as its source of students, teachers, subjects, and sections. AttendTrack calls itself "ABC" in its own README. It is a PHP 8 and MySQL application on the school's cPanel host, and its source is the `attendtrack` repository.

### Records pulled from AttendTrack

| Record | AttendTrack table | Fields ClearPath keeps | Used for |
| --- | --- | --- | --- |
| Students | `students` | `student_no` (school-issued number, the primary key), `full_name`, `section_id`, `gender`, `program`, `date_enrolled`, `active` | Student lookup fields on request forms; request search and details |
| Teachers and staff | `users` | `user_id`, `email`, `full_name`, `role` (superadmin, admin, adviser, teacher), `bands` (GS, JHS, SHS), `active` | Teacher lookup fields; matching ClearPath people to staff by sign-in email |
| Subjects | `subjects` | `subject_id`, `name`, `active` | Subject lookup fields |
| Sections | `sections` | `section_id`, `grade`, `section_name`, `label` (for example "Grade 7 - A"), `school_year` | Grouping students by class; scoping the sync to the current school year |
| Class assignments | `gradesheets` | `school_year`, `section_id`, `subject_id`, `teacher_user_id` | Knowing which teacher handles a subject for a section (Grades 1 to 10 only, where a gradesheet exists) |

Not pulled: guardian emails and phones, households, RFID tags, student emails, password hashes, parent QR tokens, attendance marks, and grades. These are personal data under the Data Privacy Act (RA 10173) or credentials, and ClearPath has no use for them.

### What AttendTrack offers today

- Staff sign in with email and password. A PHP session cookie and a CSRF token guard every call to `api.php`. There is no API key or service account.
- Read actions exist for subjects (`listSubjects`), sections (`listSections`), and one section's roster (`attnRoster`, which also requires a date and a subject). Listing staff (`listUsers`) needs the `users` permission, which only superadmins hold.
- Band-limited teachers see only their own sections, enforced server-side.
- The `adviser` role exists, but nothing fills the homeroom link (`users.section_id`) or the section's `adviser_email`. Both columns are empty after import.

ClearPath cannot sync from these actions without holding a superadmin login and driving a browser session, so the plan adds one purpose-built export instead.

### How the connection will work

1. AttendTrack gains a read-only export action in `api.php` (for example `action=directoryExport`) that returns active sections, students, subjects, staff, and class assignments for the current school year in one JSON response. It is guarded by a long random bearer token stored in AttendTrack's `config.php` next to `hmac_secret`, not by a staff session. It is GET-only, changes nothing, and writes one audit row per call.
2. ClearPath stores the AttendTrack base URL as a Worker variable and the bearer token as a Worker secret. The Worker calls the export on a schedule (Cron Trigger) and on demand from the admin screen. The browser never talks to AttendTrack.
3. The Worker upserts the records into read-only reference tables in D1, keyed by AttendTrack's own IDs, and records the sync time and row counts.
4. Request forms offer these records through searchable lookup fields: a student by name or number, a teacher by name, a subject by name. A student's section and grade follow automatically.
5. A request stores the AttendTrack ID and display name at submission time. Later roster changes do not rewrite an existing request.
6. Approval routing can use the matched records once AttendTrack fills the homeroom link, for example routing a step to the student's section adviser, matched to a ClearPath person by email.

### Sync and failure handling

- One request per sync. The live dataset holds 2,060 students and 180 staff, which fits in one response and one D1 batch.
- If AttendTrack is unreachable or returns an error, ClearPath keeps the last synced records and shows the last successful sync time on the admin screen.
- Records missing from the export, or marked inactive in AttendTrack, become inactive in ClearPath rather than deleted, so existing requests keep their references. Lookup fields offer active records only.
- Sections carry a `school_year`. At year rollover the sync picks up the new sections and retires the old ones.
- AttendTrack stores all dates in Asia/Manila local time. ClearPath records sync timestamps in UTC and converts for display.

### Changes required in AttendTrack

- Add the export action and its bearer-token check in `api.php`, with the token key in `config.sample.php`.
- Start recording each adviser's homeroom section, or the section's adviser email, so routing to advisers has data to work from.
- No schema change is needed for the export itself.

### Open items to confirm

- The AttendTrack production base URL and who will add the token to its `config.php`.
- Whether staff emails in AttendTrack match the sign-in emails ClearPath receives from the private Site. If not, a mapping table is needed before matching by email can work.
- Whether class assignments are also needed for sections without gradesheets (preschool and Senior High). The attendance records table holds who recorded each class and could supply them.
- Sync interval. Hourly during enrollment and nightly otherwise is the suggested default.

## Technical outline

- Frontend and API: React/Vinext application running as a Cloudflare Worker
- Structured persistence: D1/SQLite with generated Drizzle migrations
- File persistence: R2 object storage
- Identity: OpenAI authenticated-user headers supplied by the private Site
- Authorization: Server-side role, permission, requester, and current-assignment checks
- Reference data: AttendTrack (ABC) records (students, teachers, subjects, sections) pulled by a server-side sync and cached in D1
- Source project: `approval-flow`

Core data includes request types, workflows, roles, people, approval requests, approval steps, activity, notifications, attachment metadata, and synced AttendTrack reference records (students, teachers, subjects, and sections).

## Administration checklist

Before broader rollout:

1. Review every request type and disable unused defaults.
2. Confirm each workflow's role assignment and response window.
3. Assign the correct role to every approver using their exact sign-in email.
4. Submit a test request for each workflow and complete its full approval route.
5. Verify that unauthorized users cannot see or download another request's files.
6. Decide who should have access to the private Site before expanding the audience.
7. Enter the AttendTrack connection, run a full sync, and spot-check one student, one teacher, and one subject against AttendTrack.
8. Confirm that each request type's lookup fields point to the correct AttendTrack record type.

## Operational notes

- Database changes are delivered through ordered migration files. Applied migrations must not be edited; future schema changes should add a new migration.
- Workflow deadlines are elapsed-hour targets. They do not currently pause for weekends or holidays.
- Overdue requests are highlighted in the inbox. Automated email delivery and scheduled escalation are not yet included.
- If an attachment upload fails after the request is created, the request remains saved and the user sees a recoverable error.
- AttendTrack reference data is read-only in ClearPath. Corrections must be made in AttendTrack and are picked up on the next sync.
- The AttendTrack bearer token lives in a Worker secret on the ClearPath side and in `config.php` on the AttendTrack side. Rotating it means updating both; no code change is needed.

## Recommended next iteration

1. Connect to AttendTrack for students, teachers, subjects, and sections (see AttendTrack (ABC) integration above).
2. Add scheduled escalations for overdue approvals.
3. Add email or Slack delivery for assignments and reminders.
4. Add reusable custom form fields per request type, including AttendTrack lookup fields.
5. Add reporting for cycle time, bottlenecks, and overdue volume.
6. Add reassignment and delegation for approver absences.

## Release verification

- Production build completes successfully.
- The release includes the D1 migration for response deadlines and attachment metadata.
- The Site configuration declares both the database and private file-storage bindings.
- The release that ships the AttendTrack integration must add the migration for the reference tables and declare the AttendTrack base URL variable and bearer-token secret.
