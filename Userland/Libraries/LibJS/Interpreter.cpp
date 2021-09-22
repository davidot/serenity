/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2020-2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ScopeGuard.h>
#include <LibJS/AST.h>
#include <LibJS/Interpreter.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/FunctionEnvironment.h>
#include <LibJS/Runtime/GlobalEnvironment.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/OrdinaryFunctionObject.h>
#include <LibJS/Runtime/Reference.h>
#include <LibJS/Runtime/Shape.h>
#include <LibJS/Runtime/Value.h>

namespace JS {

NonnullOwnPtr<Interpreter> Interpreter::create_with_existing_realm(Realm& realm)
{
    auto& global_object = realm.global_object();
    DeferGC defer_gc(global_object.heap());
    auto interpreter = adopt_own(*new Interpreter(global_object.vm()));
    interpreter->m_global_object = make_handle(&global_object);
    interpreter->m_realm = make_handle(&realm);
    return interpreter;
}

Interpreter::Interpreter(VM& vm)
    : m_vm(vm)
{
}

Interpreter::~Interpreter()
{
}

void Interpreter::run(GlobalObject& global_object, const Program& program)
{
    // FIXME: Why does this receive a GlobalObject? Interpreter has one already, and this might not be in sync with the Realm's GlobalObject.

    auto& vm = this->vm();
    VERIFY(!vm.exception());

    VM::InterpreterExecutionScope scope(*this);

    vm.set_last_value(Badge<Interpreter> {}, {});

    ExecutionContext execution_context(heap());
    execution_context.current_node = &program;
    execution_context.this_value = &global_object;
    static FlyString global_execution_context_name = "(global execution context)";
    execution_context.function_name = global_execution_context_name;
    execution_context.lexical_environment = &realm().global_environment();
    execution_context.variable_environment = &realm().global_environment();
    execution_context.realm = &realm();
    execution_context.is_strict_mode = program.is_strict_mode();
    vm.push_execution_context(execution_context, global_object);
    VERIFY(!vm.exception());
    auto value = program.execute(*this, global_object);
    vm.set_last_value(Badge<Interpreter> {}, value.value_or(js_undefined()));

    vm.pop_execution_context();

    // At this point we may have already run any queued promise jobs via on_call_stack_emptied,
    // in which case this is a no-op.
    vm.run_queued_promise_jobs();

    vm.run_queued_finalization_registry_cleanup_jobs();

    vm.finish_execution_generation();
}

GlobalObject& Interpreter::global_object()
{
    return static_cast<GlobalObject&>(*m_global_object.cell());
}

const GlobalObject& Interpreter::global_object() const
{
    return static_cast<const GlobalObject&>(*m_global_object.cell());
}

Realm& Interpreter::realm()
{
    return static_cast<Realm&>(*m_realm.cell());
}

const Realm& Interpreter::realm() const
{
    return static_cast<const Realm&>(*m_realm.cell());
}

Value Interpreter::execute_statement(GlobalObject& global_object, const Statement& statement, ScopeType scope_type)
{
    if (!is<ScopeNode>(statement))
        return statement.execute(*this, global_object);

    auto& block = static_cast<const ScopeNode&>(statement);
    auto is_block = is<BlockStatement>(block);
    Vector<FlyString> const& labels = [&] {
        if (is_block)
            return static_cast<BlockStatement const&>(block).labels();
        else
            return Vector<FlyString>();
    }();

    Environment* old_environment = vm().running_execution_context().lexical_environment;
    ScopeGuard restore_environment = [&] {
        vm().running_execution_context().lexical_environment = old_environment;
    };

    if (is_block && scope_type != ScopeType::Function) {
        // Function, Eval and Script/Module should have already done the necessary steps
        auto* block_environment = new_declarative_environment(*old_environment);
        static_cast<BlockStatement const&>(block).block_declaration_instantiation(global_object, block_environment);
        vm().running_execution_context().lexical_environment = block_environment;
    }

    Value last_value;
    for (auto& node : block.children()) {
        auto value = node.execute(*this, global_object);
        if (!value.is_empty())
            last_value = value;
        if (vm().should_unwind()) {
            if (!labels.is_empty() && vm().should_unwind_until(ScopeType::Breakable, labels))
                vm().stop_unwind();
            break;
        }
    }

    if (scope_type == ScopeType::Function) {
        bool did_return = vm().unwind_until() == ScopeType::Function;
        if (!did_return)
            last_value = js_undefined();
    }

    if (vm().unwind_until() == scope_type)
        vm().stop_unwind();

    return last_value;
}

FunctionEnvironment* Interpreter::current_function_environment()
{
    return verify_cast<FunctionEnvironment>(vm().running_execution_context().lexical_environment);
}

}
