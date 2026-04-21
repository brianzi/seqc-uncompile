// ============================================================================
// Minimal stub for SeqCAstNode — needed for unique_ptr<SeqCAstNode> destruction
// TODO: Full reconstruction pending
// ============================================================================
#pragma once

#include <memory>
#include <string>

namespace zhinst {

class SeqCAstNode {
public:
    virtual ~SeqCAstNode() = default;
    virtual std::unique_ptr<SeqCAstNode> clone() const = 0;
};

} // namespace zhinst
