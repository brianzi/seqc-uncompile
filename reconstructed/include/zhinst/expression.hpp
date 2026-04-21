// ============================================================================
// Minimal stub for Expression — needed for shared_ptr<Expression>
// TODO: Full reconstruction pending
// ============================================================================
#pragma once

#include <memory>

namespace zhinst {

class Expression {
public:
    virtual ~Expression() = default;
};

} // namespace zhinst
