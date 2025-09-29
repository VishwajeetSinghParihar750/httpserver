#pragma once

class task
{
public:
    virtual ~task() {}
    virtual void execute() = 0;
};

// u can use future promise stuff without passing direct arsg to task execute , just use lambda state and pararms
template <typename F>
class callableTask : public task
{
    F function_;

public:
    callableTask(F &&function) : function_(std::forward<F>(function)) {}

    void execute() override { function_(); }
};
