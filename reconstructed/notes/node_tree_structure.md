# Node Tree Structure {#notes_node_tree_structure}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

Reconstructed from `Node::insertBefore`, `updateParent`, `remove`, `swap`,
`last`, `clone`, `toJson`, `fromJson`, `installPointers` at addresses
0x1cd860–0x269020.

## Overview

Nodes form a tree with three kinds of child links:

```
         ┌──────────┐
         │  parent   │  (weak_ptr, +0xF0)
         │  (Loop)   │
         └─────┬─────┘
               │
     next chain (+0xB8, shared_ptr):
               │
         ┌─────▼─────┐      next      ┌──────────┐      next      ┌──────────┐
         │  Node A    │──────────────►│  Node B    │──────────────►│  Node C    │──► nullptr
         │  (Branch)  │               │  (Play)    │               │  (Load)    │
         └─────┬──────┘               └────────────┘               └────────────┘
               │
               ├── loop (+0xE0, shared_ptr)
               │         │
               │   ┌─────▼─────┐
               │   │  loop node │
               │   └────────────┘
               │
               └── branches (+0xC8, vector<shared_ptr>)
                         │
                   ┌─────▼─────┐  ┌────────────┐
                   │  child 0   │  │  child 1   │  ...
                   └────────────┘  └────────────┘
```

## Link Fields

| Offset | Type                      | Name     | JSON Key    | Role                                      |
|--------|---------------------------|----------|-------------|--------------------------------------------|
| +0xB8  | `shared_ptr<Node>`        | next     | "next"      | Next node in sibling chain (linked list)   |
| +0xC8  | `vector<shared_ptr<Node>>`| branches | "branches"  | Child nodes (only used for Branch type=4)  |
| +0xE0  | `shared_ptr<Node>`        | loop     | "loop"      | Loop/else branch link                      |
| +0xF0  | `weak_ptr<Node>`          | parent   | "parent"    | Back-pointer to parent (weak to avoid cycles)|

## How Each Method Uses the Links

### `last(node)` — 0x1d5cb0
Follows `node->next` chain until null. Returns the tail of the
sibling list.

### `insertBefore(newNode)` — 0x1cd860
Inserts `newNode` before `this` in the sibling chain:
1. `newNode->next = shared_from_this()`
2. `newNode->parent = this->parent`
3. `updateParent(parent, this, newNode)` — parent now points to newNode
4. `this->parent = newNode` — this node's new "parent" is newNode

After: `... → newNode → this → ...`

### `updateParent(parent, oldChild, newChild)` — 0x1d2f50
Core dispatch that replaces `oldChild` with `newChild` in whichever slot
of `parent` currently holds it. Checks in order:

1. `parent->next == oldChild` → replace
2. If `parent->type == Branch (4)`: scan `parent->branches` vector
   - If `newChild` is non-null: replace in-place
   - If `newChild` is null: erase the element (shifts vector)
3. `parent->loop == oldChild` → replace

Then sets `newChild->parent = parent` (as weak_ptr).

### `remove(node)` — 0x1d4440
Removes `node` from the tree:
1. If `node->next` exists: splice it into parent's slot via
   `updateParent(parent, node, node->next)`, then clear next
2. Else: `updateParent(parent, node, nullptr)` — just remove
3. Recursively `remove(node->loop)` if present
4. Recursively `remove` each element in `node->branches`

### `swap(a, b)` — 0x1d2720
**Precondition:** `b->parent.get() == a.get()` — a must be b's parent.

1. Walk up from `a` through Loop (type 8) and Branch (type 4) ancestors
   to find the first non-Loop/non-Branch ancestor
2. Copy that ancestor's `asmId` to `b` if > 0
3. Save `bNext = b->next`
4. Three `updateParent` calls:
   - `updateParent(parentOfA, a, b)` — b takes a's slot in a's parent
   - `updateParent(b, bNext, a)` — inside b, a replaces bNext
   - `updateParent(a, b, bNext)` — inside a, bNext replaces b

Net effect: b is promoted to a's position; a becomes a child/sibling of b.
Throws error 0xa4 if precondition fails.

### `clone()` — 0x1d5d40
Creates a **shallow structural copy** — copies data fields but NOT tree
links. Specifically copies: `deviceIndex`, `wavesPerDev`, `play`,
`lengthReg`, `indexOffsetReg`, `tableIndex`, `trig`. Does NOT copy:
`config`/`currentCwvf`, `next`, `loop`, `branches`, `parent`, `globalRate`,
`defaultPrecompFlags`, `loopBodyRunsAtLeastOnce`, `branchMaySkipAllBodies`.

### `toJson(idMap)` — 0x264b90
Serializes all fields to a `boost::json::value` object with 21 key-value
pairs. Pointer fields serialized as integer IDs (-1 = null). The `idMap`
remaps `nodeId` values to serializable indices.

### `fromJson(json)` — 0x268280 (static)
Deserializes scalar fields from JSON, constructing a Node via the full
constructor. Pointer fields left empty — reconnected by `installPointers()`.

### `installPointers(nodeMap, json)` — 0x269020
Reconnects pointer fields after deserialization:
- `play` from `json["play"]` array (int IDs → weak_ptr via nodeMap)
- `next` from `json["next"]` (int ID → shared_ptr)
- `branches` from `json["branches"]` array (int IDs → shared_ptr)
- `loop` from `json["loop"]` (int ID → shared_ptr)
- `parent` from `json["parent"]` (int ID → weak_ptr)

## Observations

- The sibling chain (`next`) is the primary sequencing mechanism —
  instructions execute in sibling order.
- Only `Branch` nodes (type 4) use the `branches` vector. Other node types
  use `next` and optionally `loop`.
- `parent` is a `weak_ptr` to break reference cycles (parent→child is
  `shared_ptr`, child→parent is `weak_ptr`).
- `swap` is not a symmetric swap — it's a "promote child to parent's
  position" operation. The name is misleading.
- `play` (+0xA0) is populated externally (not by any Node method we've
  seen). It may hold play-related dependency links from the optimizer.
  JSON key "play" confirms the field's semantic role.
