# Design: Signatures, Verification, and Structured Requests

Status: Draft
Applies to: ClearPath Approvals (`approval-flow`)

## Summary

Add these related capabilities to ClearPath:

1. **Internal signatures** — approvers on selected workflow steps formally sign when they approve.
2. **Verifiable approval certificates** — when a request completes, ClearPath generates a PDF certificate carrying a QR code and a short reference code that link back to the durable approval record.
3. **Separation of duties** — server-side rules that stop anyone, including administrators, from approving their own requests or quietly reshaping the rules.
4. **Structured requests** — custom form fields per request type (including item tables and computed totals), routing that depends on field values, and post-approval fulfillment tracking. This makes the flow work for requests such as supplies, not just documents.

Signers are **internal** only. All users are already authenticated through the private Site, so no third-party e-signature provider is needed.

## Goals

- Let administrators require a signature on specific workflow steps.
- Bind each signature to the signer's identity, the time, and the exact request details and attachments.
- Produce a downloadable approval certificate for completed requests.
- Let anyone with Site access scan the certificate's QR code and confirm it matches the stored record.
- Detect attachments or request details that changed after signing.
- Prevent self-approval and single-person control over a request's outcome.
- Support form-based requests (for example supply requests) with amount-based routing.
- Track what happens after approval, and prevent an approved request from being fulfilled twice.

## Non-goals

- Signatures from external parties (vendors, clients). A provider integration can be added later if needed.
- Certificate-based digital signatures embedded in the PDF (PAdES / PKI).
- Public, unauthenticated verification. Verification stays behind the private Site.
- Inventory or purchasing management. Fulfillment tracking records status only.

## Part 1 — Internal signatures

### Configuration

Each workflow step gets a `Requires signature` option. It is copied into the durable approval step at submission time, like the rest of the route, so later workflow edits do not affect existing requests.

### Signing flow

1. On a step that requires a signature, **Approve** opens a signing panel.
2. The approver draws or types a signature, or reuses one saved on their profile.
3. The approver confirms an attestation: "I approve this request and the attached documents."
4. The server runs the authorization and separation-of-duties checks (Part 3), then records the signature and the approval decision in a single transaction.

Returns do not require a signature.

### Captured evidence

| Field | Source |
| --- | --- |
| Signer email and display name | Site authenticated-user headers |
| Signer role | ClearPath role at time of signing |
| Signed at | Server timestamp (UTC) |
| IP address and user agent | Request headers |
| Content hash | SHA-256 of the canonical request details, including custom field values and line items |
| Attachment hashes | SHA-256 of each attachment's bytes |
| Signature image | PNG stored in R2 |
| Attestation text | Exact text shown to the signer |

The content hash is computed over a canonical JSON serialization (sorted keys, normalized numbers and dates) so the same details always produce the same hash.

### Content immutability

Once any step on a request has been signed, its details and attachments are locked. Changing them requires either:

- a **return** to the requester, which invalidates prior signatures and restarts the route at step 1, or
- an **approver amendment** (Part 4), which creates a new signed version.

## Part 2 — QR verification

### Approval certificate

When the final step is approved, ClearPath generates a PDF certificate and stores it in R2 alongside the request's attachments. It contains:

- Request title, type, requester, and submission date
- Request details, including custom fields and the line-item table with totals
- Each approval step: approver name, role, decision time, and signature (where required); skipped conditional steps are listed with the reason
- Attachment names with their SHA-256 hashes (shortened for display, full value in the record)
- A **QR code** encoding the verification URL
- A **reference code** printed beside the QR code for manual lookup
- A footer: "System-generated approval certificate. Verify at <URL> or with reference <code>."

The certificate uses the same download authorization as other attachments.

### Verification token and reference code

- **Verification token**: 128-bit random value, base64url encoded, generated when the certificate is issued. The QR code encodes `https://<site>/verify/<token>`.
- **Reference code**: short human-readable code (for example `CP-2026-7K4M-Q9XD`) for typing or reading aloud. It is also random, not a sequential request ID, so codes cannot be enumerated.

Sequential numbers (like a plain `1078` in a Code 39 barcode) are avoided because they are guessable and prove nothing without a manual lookup.

### Verification page

`/verify/<token>` and `/verify?ref=<code>` are protected by the private Site, like the rest of the app. The page shows:

- **Certificate status**: Valid, Superseded, or Revoked
- **Fulfillment status** where the request type tracks it (Part 5), for example "Released on 2026-10-10 — already fulfilled"
- Request title, type, requester, and completion date
- Request details and line items
- Approval steps with approver names, roles, and decision times
- Attachment names and hashes

Attachment downloads from this page follow the existing rules (administrators, requester, and assigned approvers). Viewers without those rights see metadata only.

### Integrity check

The verification page offers **Verify integrity**. The server re-hashes the stored attachment bytes in R2 and the stored request details, and compares them with the hashes recorded at signing. Any mismatch is shown prominently and written to the activity trail.

A user holding a printed or emailed certificate can also compare a file's SHA-256 against the value on the certificate without contacting anyone.

### Certificate status

| Status | When |
| --- | --- |
| Valid | Certificate is the latest for a completed request |
| Superseded | A newer certificate was issued for the same request |
| Revoked | An administrator revoked it, with a required reason |

Revocation and supersession are recorded in the activity trail. Tokens are never reused.

### Barcode choice

QR code over a 1D barcode because it holds a full URL, is readable by any phone camera without a dedicated app, and has error correction that tolerates printing and scanning. Use error-correction level M.

## Part 3 — Separation of duties

### Risks in the current model

Based on `HANDOFF.md`:

1. Administrators can both submit and approve, and nothing prevents a user from approving their own request.
2. Administrators can edit workflows and roles, so they can route a request to themselves.
3. Roles are assigned by email; a mis-assignment silently grants approval authority.
4. Every active request type appears to be available to every requester.
5. A certificate proves who approved what; it does not prove the content is true. Approvers must still review it.

### Rules

| Rule | Enforcement |
| --- | --- |
| **No self-approval** | The server rejects any decision where the actor is the request's requester, including administrators. |
| **One decision per person** | A person may decide at most one step of the same request. |
| **Submit restrictions** | Each request type has an optional list of roles allowed to submit it. The submission form only shows permitted types, and the server re-checks on submit. |
| **Admin change audit** | Every change to request types, workflows, roles, and role assignments is written to an audit log with actor, time, and before/after values. |
| **Two-person rule for sensitive config** (optional) | Request types can be marked sensitive. Changes to their workflows, and role assignments that grant approval on them, take effect only after a second administrator approves the change. |
| **Break-glass override** | If a step genuinely has no eligible approver, a second administrator can authorize an override. It requires a reason and is recorded on the request and in the audit log. |
| **Visible participants** | Certificates and verification pages always show the requester and every approver. |

If a step's only eligible approver is the requester, the step is left pending and administrators are notified to assign another approver or use the break-glass override.

## Part 4 — Structured requests

### Custom form fields

Each request type defines its own form. Field definitions are versioned; a submitted request stores the field definition version it used, so later form edits do not change existing requests.

| Field type | Notes |
| --- | --- |
| Text, long text | Optional min/max length |
| Number, currency | Currency stored as integer centavos |
| Date | |
| Select / multi-select | Admin-defined options |
| Cost center / department | Select from an admin-managed list |
| Item table | Columns such as description, quantity, unit, unit price; line totals computed |
| Computed total | Sum of an item table's line totals; always recomputed on the server |

### Conditional routing

Workflow steps can have a condition based on field values, for example `total > 10000` or `cost_center = "IT"`.

- Conditions are evaluated once, at submission, against the submitted values.
- Steps whose condition is false are recorded as **Skipped (condition not met)** and shown in the route, the activity trail, and the certificate.
- Conditions use a small fixed set of operators (`=`, `!=`, `>`, `>=`, `<`, `<=`, `in`), not free-form code.

### Approver amendments (optional)

An approver on a step that requires a signature may reduce quantities or remove line items instead of returning the request.

- The amendment creates a new request version; the previous version is kept.
- The amending approver's signature covers the new content hash.
- The requester is notified with a diff of the changes.
- Increases are not allowed; they require a return so earlier approvers see the higher amount.
- If the amended total falls below a later step's threshold, that step stays on the route; the route is not recalculated after submission.

## Part 5 — Fulfillment tracking

Request types can enable post-approval statuses. Default set for supply requests:

`Approved → Ordered → Received → Released`

- Each transition records actor, time, and an optional note in the activity trail.
- Allowed actors are configured per request type (for example Procurement for Ordered/Received, Storeroom for Released).
- Once a request reaches its final fulfillment status, the verification page shows it as **Already fulfilled** with the date, so a copied certificate cannot be used to claim items twice.
- Partial fulfillment is recorded per line item where the request type uses an item table.

## Example flows

### A. Official letter

**Request type:** Official Letter (submit restricted to Staff and Manager roles)

| Step | Role | Signature | Response window |
| --- | --- | --- | --- |
| 1 | Department Head | Required | 24h |
| 2 | Legal / Compliance | Not required | 48h |
| 3 | Director | Required | 24h |

1. **Submit.** The requester fills in subject, recipient, and purpose, and uploads the letter. The server checks submit permission, copies the workflow into durable steps, stores files in R2 with SHA-256 hashes, activates step 1, and notifies the Department Head role.
2. **Step 1 — Department Head.** Return sends the request back with a note; resubmission restarts at step 1 and clears signatures. Approve opens the signing panel. The server checks the actor is the assigned approver, is not the requester, and has not decided another step; re-verifies hashes; records signature and approval atomically; locks content; activates step 2.
3. **Step 2 — Legal.** Same checks, no signing panel. Missing the response window marks the request overdue.
4. **Step 3 — Director.** Signs and approves; the request becomes Approved.
5. **Certificate.** Generated with request details, steps, signatures, file hashes, QR code, and reference code. The requester is notified.
6. **Verification.** A recipient scans the QR code, sees the status and approval history, and can run the integrity check.
7. **Corrections.** An administrator revokes the certificate with a reason; a new submission produces a new certificate and the old one shows as Superseded.

### B. Supply request

**Form:**

| Field | Example |
| --- | --- |
| Department / cost center | Operations – OPS-01 |
| Needed by | 2026-10-15 |
| Items | 10 × Bond paper A4 @ ₱250 · 5 × Toner HP 26A @ ₱4,200 · 2 × Office chair @ ₱6,500 |
| Total (computed) | ₱36,500 |
| Purpose | Q4 replenishment + 2 new hires |
| Attachment (optional) | Supplier quotation PDF |

**Route:**

| Step | Role | Condition | Signature |
| --- | --- | --- | --- |
| 1 | Immediate Supervisor | Always | No |
| 2 | Budget / Finance | Total > ₱10,000 | Required |
| 3 | Department Head / Director | Total > ₱50,000 | Required |
| 4 | Procurement | Always | No |

For ₱36,500 the route is Supervisor → Finance → Procurement; the Director step is recorded as skipped.

1. **Submit.** The server recomputes line totals and the grand total, hashes the details and any quotation, evaluates step conditions, and notifies the Supervisor.
2. **Supervisor.** Approves, or returns with a note (for example "reduce chairs to 1").
3. **Finance.** Signs and approves, locking items and total. Optionally amends downward (for example toner 5 → 3), creating a new signed version and notifying the requester.
4. **Procurement.** Approves; the request becomes Approved.
5. **Certificate.** "Approved Supply Request" with the item table, total, cost center, approvers, signatures, QR code, and reference code.
6. **Fulfillment.** Procurement marks Ordered and Received; Storeroom scans the QR code, confirms the certificate is Valid and items match, releases the items, and marks Released. Any later scan shows **Already fulfilled**.

### Flow overview

```
Requester submits ─▶ [submit permission, server totals, hashes, route conditions]
   │
   ▼
Step N approver ── return ──▶ back to requester (signatures cleared)
   │ approve (+ sign if required)
   │ [assigned? not requester? no prior decision? content unchanged?]
   ▼
More steps? ── yes ──▶ next step (skipped steps recorded)
   │ no
   ▼
Approved ─▶ Certificate PDF (QR + reference code) ─▶ requester notified
   │
   ▼
Fulfillment (optional): Ordered ─▶ Received ─▶ Released
   │
   ▼
Scan QR ─▶ Verify page: Valid / Superseded / Revoked, fulfillment status, integrity check
```

## Data model

Delivered as **new** D1 migrations. Existing migrations are not edited.

**Signatures and certificates**

- `workflow_steps.requires_signature` — boolean, default false
- `approval_steps.requires_signature` — boolean, copied at submission
- `signatures`
  - `id`, `request_id`, `request_version_id`, `approval_step_id`
  - `signer_email`, `signer_name`, `signer_role`
  - `signed_at`, `ip_address`, `user_agent`
  - `content_hash`, `attestation_text`, `signature_image_key` (R2)
- `signature_attachment_hashes`
  - `signature_id`, `attachment_id`, `sha256`
- `approval_certificates`
  - `id`, `request_id`, `token` (unique), `reference_code` (unique)
  - `pdf_key` (R2), `status`, `issued_at`
  - `revoked_at`, `revoked_by`, `revocation_reason`, `superseded_by`

**Separation of duties**

- `request_types.allowed_submitter_role_ids` — JSON array, empty means all requesters
- `request_types.is_sensitive` — boolean
- `admin_audit_log`
  - `id`, `actor_email`, `entity_type`, `entity_id`, `action`, `before_json`, `after_json`, `created_at`
- `pending_config_changes` (two-person rule)
  - `id`, `requested_by`, `entity_type`, `entity_id`, `change_json`, `status`, `approved_by`, `decided_at`
- `approval_overrides`
  - `id`, `request_id`, `approval_step_id`, `authorized_by`, `reason`, `created_at`

**Structured requests**

- `form_definitions`
  - `id`, `request_type_id`, `version`, `fields_json`, `created_at`
- `request_versions`
  - `id`, `request_id`, `version`, `form_definition_id`, `values_json`, `total_centavos`, `content_hash`, `created_by`, `created_at`, `reason`
- `request_line_items`
  - `id`, `request_version_id`, `position`, `description`, `quantity`, `unit`, `unit_price_centavos`, `line_total_centavos`
- `workflow_steps.condition_json` — nullable
- `approval_steps.status` gains `skipped`, with `skip_reason`

**Fulfillment**

- `request_types.fulfillment_statuses_json` and allowed actor roles per status
- `fulfillment_events`
  - `id`, `request_id`, `line_item_id` (nullable), `status`, `quantity` (nullable), `actor_email`, `note`, `created_at`

## Implementation notes

- Signature pad: `signature_pad`
- PDF generation in the Worker: `pdf-lib`
- QR generation: a pure-JS encoder such as `qrcode-generator`, rendered to PNG or drawn as vector paths in the PDF
- Hashing: Web Crypto `crypto.subtle.digest('SHA-256', …)`; stream large attachments from R2
- Token generation: `crypto.getRandomValues`
- Money: integer centavos everywhere; format only for display
- Form rendering: generate the form from `fields_json`; validate on the server against the same definition
- Authorization: every decision, amendment, fulfillment transition, and admin change is authorized on the server

## Security considerations

- Tokens and reference codes are random and unguessable; verification endpoints are rate limited.
- Verification pages remain behind private Site authentication.
- Signing and approval are atomic, so a signature cannot exist without its decision.
- Signature images are stored privately and only rendered for authorized viewers.
- Hashes and totals are computed server-side, never trusted from the client.
- Separation-of-duties rules apply to administrators as well as other users.
- Admin configuration changes are audited; sensitive changes can require a second administrator.

## Legal note

Electronic signatures are recognized under the Philippine E-Commerce Act (RA 8792) when the signer's identity and the document's integrity can be demonstrated. The Site identity, captured evidence, content hashes, and verifiable certificate are intended to support both. Confirm requirements with legal counsel for any document types with specific signing rules.

## Rollout

1. Separation-of-duties rules (no self-approval, one decision per person, submit restrictions, admin audit log). These close existing gaps and ship first.
2. Custom form fields, item tables, and computed totals.
3. Conditional routing.
4. Signature capture behind a per-step setting, off by default.
5. Certificate generation, verification page, and integrity check.
6. Certificate revocation and supersession.
7. Fulfillment tracking.
8. Optional: approver amendments and the two-person rule for sensitive configuration.
9. Pilot on one letter type and one supply type, then enable more broadly.

## Open questions

- Should saved profile signatures be allowed, or must approvers draw each time?
- Should certificates be generated for requests that completed before this feature shipped?
- Should the verification page show approver names to all Site users, or only to administrators and participants?
- Which request types should be marked sensitive and use the two-person rule?
- Are approver amendments needed, or is returning the request enough?
- Who owns the cost center list, and does it need to sync with an external system?
