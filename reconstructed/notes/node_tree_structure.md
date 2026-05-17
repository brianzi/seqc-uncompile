# Node Tree {#notes_node_tree_structure}

The **node tree** is the structural IR the front-end produces
during lowering (step 4 of \ref notes_pipeline).  It captures
the program's nested loops, branches, and play operations as a
tree, and is consumed by the prefetcher and the linearisation
step to emit assembly in execution order.

Each `Node` carries:

- **Local data** — opcode-like fields specific to the node kind
  (device index, waveform indices, register handles, trigger
  bits, …).
- **Tree links** — pointers to other nodes that define program
  shape.

The node-kind set is small: `Branch`, `Loop`, `Play`, `Load`,
and a handful of leaf control-flow kinds.  All kinds share the
same `Node` class and tree-link layout.

[TOC]

## Three kinds of child links

```
         ┌───────────┐
         │  parent    │  (weak, set by updateParent)
         └─────┬──────┘
               │
   next chain (shared, forward only):
               │
         ┌─────▼─────┐  next   ┌──────────┐  next   ┌──────────┐
         │  Node A    │────────►│  Node B   │────────►│  Node C   │──► nullptr
         │  (Branch)  │         │  (Play)   │         │  (Load)   │
         └─────┬──────┘         └───────────┘         └───────────┘
               │
               ├── loop   (shared, optional)
               │      └──► [loop-body node]
               │
               └── branches (vector<shared>, only for Branch kind)
                       │
                  ┌────▼────┐  ┌──────────┐
                  │ child 0 │  │ child 1  │  …
                  └─────────┘  └──────────┘
```

The link semantics, summarised:

| Link        | Cardinality      | Owner    | Purpose                                                  |
|-------------|------------------|----------|----------------------------------------------------------|
| `next`      | 0 or 1           | shared   | Next node in sibling execution order (the primary chain) |
| `branches`  | 0..N             | shared   | Children of a `Branch` node (one per case arm)           |
| `loop`      | 0 or 1           | shared   | Loop body, or "else" branch on conditional nodes         |
| `parent`    | exactly 1 (root: empty) | weak     | Back-pointer; weak so the tree owns its descendants      |

The sibling chain (`next`) is the primary execution ordering;
`branches` and `loop` introduce nested scopes.

## Why `parent` is weak

`parent` is held as `weak_ptr` so the only owning edges go
top-down (`shared_ptr` along `next`, `branches`, `loop`).
Without this, the bidirectional link would create reference
cycles and the tree would never be freed.

## Mutating operations

The node-tree API is intentionally small.  All structural edits
go through a handful of free / member functions; nothing in the
front-end manipulates the link fields directly.

### `last(node)`

Walks the `next` chain to its tail and returns it.  Used by
every operation that needs to append onto a sibling list.

### `insertBefore(newNode)`

Splices `newNode` into the chain immediately before `this`:

1. `newNode->next = shared_from_this()`
2. `newNode->parent = this->parent`
3. `updateParent(parent, this, newNode)` — the parent's pointer
   that referred to `this` now refers to `newNode`.
4. `this->parent = newNode` — `this`'s back-pointer is updated.

After: *… → newNode → this → …*.

### `updateParent(parent, oldChild, newChild)`

The central dispatch used by every structural edit.  Replaces
`oldChild` with `newChild` (which may be `nullptr`) in whichever
slot of `parent` currently holds `oldChild`.  Checked, in order:

1. `parent->next == oldChild` — replace.
2. If `parent` is a `Branch`: scan `parent->branches`; replace
   in place if `newChild` is non-null, otherwise erase the
   element (the vector shifts down).
3. `parent->loop == oldChild` — replace.

Finally `newChild->parent = parent` (as a weak reference).

### `remove(node)`

Removes `node` from the tree:

1. If `node->next` exists, splice it into `node`'s slot via
   `updateParent(parent, node, node->next)`, then clear
   `node->next`.
2. Otherwise call `updateParent(parent, node, nullptr)` — the
   slot is emptied (or, for `branches`, the element is erased).
3. Recursively `remove(node->loop)` if present.
4. Recursively `remove` each element of `node->branches`.

### `swap(a, b)`

**Precondition**: `b->parent == a` (a is b's parent).

The operation is not a symmetric swap — it *promotes* `b` to
`a`'s position and demotes `a` into `b`'s old slot.  This is the
shape needed by certain optimisation transforms; the name is
historical.

1. Walk up from `a` through any enclosing `Loop` / `Branch`
   ancestors to find the first non-`Loop`/non-`Branch`
   ancestor; copy its `asmId` to `b` (if positive) so the
   promoted node inherits the source-line tag.
2. Save `bNext = b->next`.
3. Three `updateParent` calls re-wire the three relevant
   pointers in one transaction:
   - `updateParent(parentOfA, a, b)` — `b` takes `a`'s slot in
     `a`'s parent.
   - `updateParent(b, bNext, a)` — inside `b`, `a` replaces
     `bNext`.
   - `updateParent(a, b, bNext)` — inside `a`, `bNext`
     replaces `b`.

Throws if the precondition fails.

### `clone()`

A **shallow structural copy** that duplicates the *data* fields
but **not** the tree-link fields.  Specifically it copies the
device index, the waves-per-device flag, the play descriptor,
the length / index-offset registers, the table index, and the
trigger bits.  It leaves `next`, `loop`, `branches`, `parent`,
and the cross-pass-only fields (config / current CWVF, global
rate, default precomp flags, loop-body-runs-at-least-once,
branch-may-skip-all-bodies) empty.

Clones are therefore safe to splice into an unrelated tree.

## JSON serialisation

The node tree round-trips through JSON for the canonical-form
validation step (step 8 of \ref notes_pipeline) and for the
debug dump emitted under bit 0x08 of the debug flags.

### `toJson(idMap)`

Emits a JSON object per node.  Pointer fields are serialised as
integer IDs through `idMap`; the sentinel `-1` represents
`nullptr`.  `idMap` remaps each node's identity onto a dense,
serialisation-friendly index.

### `fromJson(json)` and `installPointers(nodeMap, json)`

Deserialisation runs in two phases because pointer fields
require all sibling nodes to exist first:

- `fromJson` constructs every node from its scalar fields; all
  pointer fields are left empty.
- `installPointers` walks the resulting node set a second time,
  looking each integer ID up in `nodeMap` and reconnecting the
  `play`, `next`, `branches`, `loop`, and `parent` slots.

## Notes on field meaning

- **`Branch` is the only kind that uses `branches`.**  Every
  other kind leaves the vector empty and relies on `next` /
  `loop`.
- **`play` is populated externally**, not by any of the methods
  above.  It carries play-related dependency links produced by
  the optimiser / prefetcher.  The JSON serialisation uses key
  `"play"`, which is the canonical reference for the field's
  semantic role.

## See also

- \ref notes_pipeline — where the node tree is built (step 4)
  and consumed (steps 6, 9, 10).
- \ref notes_frontend_lowering — `FrontEndLoweringFacade` and
  the lowering data model that builds the tree.
- \ref notes_prefetch_scheduling — the pass that walks the tree
  to decide waveform placement.
