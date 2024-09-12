#ifndef IOPERATE_H
#define IOPERATE_H

#include <memory>
#include <string>

class OpNode; // Forward declaration

class IOperate {
public:
    virtual ~IOperate() = default;

    // Pure virtual function that must be implemented by derived classes
    virtual std::string Symbol() const = 0;
    virtual void Operate(std::shared_ptr<OpNode> node) = 0;
};

#endif // IOPERATE_H
