#ifndef IOPERATE_H
#define IOPERATE_H

#include <memory>
#include <string>

// Forward declaration of OpNode class
class OpNode;

// Definition of the IOperate interface
class IOperate {
public:
    virtual ~IOperate() = default;
    virtual std::string Symbol() const = 0;
    virtual void Operate(std::shared_ptr<OpNode> node) = 0;
};

#endif // IOPERATE_H
