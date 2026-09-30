# Design: Internal Signatures and QR Verification

Status: Draft
Applies to: ClearPath Approvals (`approval-flow`)

## Summary

Add two related capabilities to ClearPath:

1. **Internal signatures** — approvers on selected workflow steps formally sign when they approve.
2. **Verifiable approval certificates** — when a request completes, ClearPath generates a PDF certificate carrying a QR code and a short reference code that link back to the durable approval record.

Both are for **internal** signers only. All signers are already authenticated through the private Site, so no third-party e-signature provider is needed.

## Goals

- Let administrators require a signature on specific workflow steps.
- Bind each signature to the signer's identity, the time, and the exact request content and attachments.
- Produce a downloadable approval certificate for completed requests.
- Let anyone with Site access scan the certificate's QR code and confirm it matches the stored record.
- Detect attachments or request details that changed after signing.

## Non-goals

- Signatures from external parties (vendors, clients). A provider integration can be added later if needed.
- Certificate-based digital signatures embedded in the PDF (PAdES / PKI).
- Public, unauthenticated verification. Verification stays behind the private Site.

## Part 1 — Internal signatures

### Configuration

Each workflow step gets a `Requires signature` option. It is copied into the durable approval step at submission time, like the rest of the route, so later workflow edits do not affect existing requests.

### Signing flow

1. On a step that requires a signature, **Approve** opens a signing panel.
2. The approver draws or types a signature, or reuses one saved on their profile.
3. The approver confirms an attestation: "I approve this request and the attached documents."
4. The server verifies the caller is the currently assigned approver, then records the signature and the approval decision in a single transaction.

Returns do not require a signature.

### Captured evidence

| Field | Source |
| --- | --- |
| Signer email and display name | Site authenticated-user headers |
| Signer role | ClearPath role at time of signing |
| Signed at | Server timestamp (UTC) |
| IP address and user agent | Request headers |
| Content hash | SHA-256 of the canonical request details |
| Attachment hashes | SHA-256 of each attachment's bytes |
| Signature image | PNG stored in R2 |
| Attestation text | Exact text shown to the signer |

### Attachment immutability

Once any step on a request has been signed, its attachments and details are locked. Changing them requires the request to be returned, which invalidates prior signatures and restarts the route.

## Part 2 — QR verification

### Approval certificate

When the final step is approved, ClearPath generates a PDF certificate and stores it in R2 alongside the request's attachments. It contains:

- Request title, type, requester, and submission date
- Each approval step: approver name, role, decision time, and signature (where required)
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

- **Status**: Valid, Superseded, or Revoked
- Request title, type, and completion date
- Approval steps with approver names, roles, and decision times
- Attachment names and hashes

Attachment downloads from this page follow the existing rules (administrators, requester, and assigned approvers). Viewers without those rights see metadata only.

### Integrity check

The verification page offers **Verify attachments**. The server re-hashes the stored attachment bytes in R2 and compares them with the hashes recorded at signing. Any mismatch is shown prominently and written to the activity trail.

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

## Data model

Delivered as a **new** D1 migration. Existing migrations are not edited.

- `workflow_steps.requires_signature` — boolean, default false
- `approval_steps.requires_signature` — boolean, copied at submission
- `signatures`
  - `id`, `request_id`, `approval_step_id`
  - `signer_email`, `signer_name`, `signer_role`
  - `signed_at`, `ip_address`, `user_agent`
  - `content_hash`, `attestation_text`, `signature_image_key` (R2)
- `signature_attachment_hashes`
  - `signature_id`, `attachment_id`, `sha256`
- `approval_certificates`
  - `id`, `request_id`, `token` (unique), `reference_code` (unique)
  - `pdf_key` (R2), `status`, `issued_at`
  - `revoked_at`, `revoked_by`, `revocation_reason`, `superseded_by`

## Implementation notes

- Signature pad: `signature_pad`
- PDF generation in the Worker: `pdf-lib`
- QR generation: a pure-JS encoder such as `qrcode-generator`, rendered to PNG or drawn as vector paths in the PDF
- Hashing: Web Crypto `crypto.subtle.digest('SHA-256', …)`; stream large attachments from R2
- Token generation: `crypto.getRandomValues`
- Authorization: sign only as the currently assigned approver; issue certificates only on final approval; revoke only as administrator

## Security considerations

- Tokens and reference codes are random and unguessable; verification endpoints are rate limited.
- Verification pages remain behind private Site authentication.
- Signing and approval are atomic, so a signature cannot exist without its decision.
- Signature images are stored privately and only rendered for authorized viewers.
- Hashes are computed server-side from stored bytes, never trusted from the client.

## Legal note

Electronic signatures are recognized under the Philippine E-Commerce Act (RA 8792) when the signer's identity and the document's integrity can be demonstrated. The Site identity, captured evidence, content hashes, and verifiable certificate are intended to support both. Confirm requirements with legal counsel for any document types with specific signing rules.

## Rollout

1. Migration and signature capture behind a per-workflow setting, off by default.
2. Certificate generation for completed requests.
3. Verification page and attachment integrity check.
4. Administrator revocation.
5. Pilot on one request type, then enable more broadly.

## Open questions

- Should saved profile signatures be allowed, or must approvers draw each time?
- Should certificates be generated for requests that completed before this feature shipped?
- Should the verification page show approver names to all Site users, or only to administrators and participants?
