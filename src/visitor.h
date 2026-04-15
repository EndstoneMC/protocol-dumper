#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cereal/schema/SchemaDescription.h"
#include "models.h"

namespace proto {

class Visitor {
public:
    virtual ~Visitor() = default;

    void setAliases(std::unordered_map<std::string, const cereal::SchemaDescription *> aliases);
    [[nodiscard]] bool isAlias(std::string_view name) const;

    [[nodiscard]] const std::vector<model::Packet> &packets() const { return mPackets; }
    [[nodiscard]] const std::vector<std::unique_ptr<model::Class>> &classes() const { return mClasses; }

    void visit(const cereal::SchemaDescription &desc);
    void visitClass(const std::string &name, const cereal::SchemaDescription &desc);
    void visitPacket(int id, const std::string &name, const cereal::SchemaDescription &desc);

protected:
    virtual void visitScalar(const cereal::SchemaDescription &desc);
    virtual void visitEnum(const cereal::SchemaDescription &desc);
    virtual void visitObject(const cereal::SchemaDescription &desc);
    virtual void visitArray(const cereal::SchemaDescription &desc);
    virtual void visitMap(const cereal::SchemaDescription &desc);

    virtual void visitField(const std::string &name, const cereal::internal::Member &member);

private:
    void visitStructFields(const cereal::SchemaDescription &desc);

    std::vector<model::Packet> mPackets;
    std::vector<std::unique_ptr<model::Class>> mClasses;
    std::unordered_map<std::string, const cereal::SchemaDescription *> mAliases;

    model::Class *mCurrentClass = nullptr;
    model::Packet *mCurrentPacket = nullptr;
    std::unique_ptr<model::FieldType> *mTypeSlot = nullptr;
};

}  // namespace proto
