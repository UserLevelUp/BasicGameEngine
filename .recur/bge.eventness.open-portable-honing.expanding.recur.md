# bge.eventness.open-portable-honing.expanding

Date: 2026-05-31
Priority: high
Focus: keep eventness and UI-honing methods portable, inspectable, and shareable across tools.

## Interesting Eventness

The `10-suitors` UI-honing method is powerful because it turns subjective judgment into recorded product knowledge. That power should belong to the user and the project, not disappear inside one assistant, vendor service, or closed workflow.

This lane is the anti-capture principle: use LLMs, component tools, feature flags, debug menus, and product platforms when useful, but keep the eventness record portable enough that the method can move with the user.

## Completed Reality

A honing technique is healthy when the user can inspect it, share it, replay it, and continue it with another tool or collaborator.

The eventness record should preserve:

- The concern being honed.
- The candidate group and selector shape.
- The human judgment pass: usability engineer, product owner, end client.
- The winning preference and rejected candidates.
- The code/config/recipe path that implements the choice.
- The checkpoint where the recorded preference is saved, committed, tagged, or versioned.

## Portability Rules

- Do not require a proprietary server to understand why a UI choice was made.
- Keep source files pure and store eventness in mirrored `.recur` lanes when possible.
- Prefer plain text records that can be read by humans and ordinary tools.
- Let LLMs assist with candidate generation and implementation, but keep the selection history outside the model's private context.
- Integrate with existing tools instead of forcing a replacement stack.
- Treat the user's recorded preference as project knowledge, not assistant memory only.

## Selective Disclosure

Open eventness is not naive transparency. Some lanes should be private, some can be shared with a collaborator, and some should become a public interface or standard.

Useful split:

- Private lane: motives, unfinished strategy, sensitive customer context, hidden constraints, credentials, security-sensitive details, or personal notes.
- Shared work lane: the task boundary, current state, accepted inputs, expected outputs, candidate group, owner, deadline, and verification method.
- Public standard lane: the schema, lifecycle states, naming convention, interoperability rules, and examples that let different users, sessions, tools, and companies coordinate.

The shareable eventness envelope should let another person continue the work without exposing every reason the lane exists. A collaborator often needs the interface, acceptance criteria, and current trace, not the whole private history.

## Open Standard Shape

Eventness becomes more useful when it can cross session and company boundaries without becoming a capture mechanism.

Minimum portable shape:

- Stable hierarchical name.
- State: expanding, collapsing, complete, blocked, or archived.
- Focus: what interest/lane is active.
- Current reality: what is known now.
- Desired completed reality: what makes the lane done.
- Handoff boundary: what can be shared, what stays private, and who can act.
- Verification: command, test, review, screenshot, user judgment, or other proof.
- Collapse record: winning choice, rejected candidates, follow-up lane, and version checkpoint.

## Sustainable Ownership

Portability should protect the user's agency and livelihood. Open eventness can define the envelope and interoperability rules while private lanes, commercial tooling, client-specific work, and original implementation remain protected or licensed.

The standard should make it easy to share enough for collaboration without forcing the creator to donate every technique, preference history, private lane, or product advantage to a closed platform.

## Boundary

Do not claim a specific company is malicious without evidence. The engineering concern is enough: any closed system that traps the honing record can weaken user agency, team continuity, and competitive portability.

## Related Lanes

- `bge.ui-honing.10-suitors`: the grouped candidate method that needs portability.
- `bge.rendering.high-framerate-multi-object-animation`: a technique captured as portable eventness instead of hidden model context.
- `space-rocks.player-icon.visibility-trials`: concrete proof that a user preference can be recorded and locked in.
- `bge.eventness.sustainable-ownership`: livelihood, licensing, and fair-exchange boundaries for open eventness.