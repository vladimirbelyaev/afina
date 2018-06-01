#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {
    void Engine::Store(context &ctx) {
        char stack;
        ctx.Low = ctx.High = StackBottom;
        if (ctx.Low > &stack){
            ctx.Low = &stack;
        }else{
            ctx.High = &stack;
        }
        uint32_t stack_size = ctx.High - ctx.Low;
        if (stack_size > std::get<1>(ctx.Stack)) {
            delete[] std::get<0>(ctx.Stack);
            std::get<0>(ctx.Stack) = new char[stack_size];
            std::get<1>(ctx.Stack) = stack_size;
        }
        memcpy(std::get<0>(ctx.Stack), ctx.Low, stack_size);
    }

    void Engine::Restore(context &ctx) {
        char stack;
        if ((&stack >= ctx.Low) && (&stack <= ctx.High)){
            Restore(ctx);
        }

        memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
        longjmp(ctx.Environment, 1);
    }


    void Engine::yield() {
        context* routine = alive;

        if (routine == cur_routine && routine) {
            routine = routine -> next;
        }

        if (routine){
            sched(routine);
        } else {
            return;
        }
    }

    void Engine::sched(void *routine_) {
        context* ctx = reinterpret_cast<context*>(routine_);
        if (cur_routine){
            if (setjmp(cur_routine->Environment)) {
                return;
            }
            Store(*cur_routine);
        }
        cur_routine = ctx;
        Restore(*cur_routine);
    }


} // namespace Coroutine
} // namespace Afina
