#ifndef BrNode_Partition_h
#define BrNode_Partition_h

#include <ActData_BasePartition.h>
#include <ActData_BaseNode.h>
#include <ActData_Utils.h>
#include <TDF_TagSource.hxx>

//! Internal helper to access protected expandOn method.
class BrNode_NodeAccessor : public ActData_BaseNode
{
public:
    static void Expand(const Handle(ActData_BaseNode)& theNode, const TDF_Label& theLabel)
    {
        struct Accessor : public ActData_BaseNode {
            using ActData_BaseNode::expandOn;
        };
        ((Accessor*) theNode.get())->expandOn(theLabel);
    }
};

//! Flexible partition for bridge nodes that allows any ActData_BaseNode.
class BrNode_Partition : public ActData_BasePartition
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_Partition, ActData_BasePartition)

    //! \return new instance of partition.
    static Handle(BrNode_Partition) Instance()
    {
        return new BrNode_Partition();
    }

    //! \return generic node type.
    Handle(Standard_Type) GetNodeType() const override
    {
        return STANDARD_TYPE(ActData_BaseNode);
    }

    //! Adds node to partition without strict type name check.
    ActAPI_DataObjectId AddNode(const Handle(ActAPI_INode)& theNode) override
    {
        if ( theNode.IsNull() ) return "";

        TDF_Label partitionLab = this->RootLabel();
        TDF_Label nodeLab = TDF_TagSource::NewChild(partitionLab);

        Handle(ActData_BaseNode) BN = Handle(ActData_BaseNode)::DownCast(theNode);
        if ( !BN.IsNull() )
        {
            BrNode_NodeAccessor::Expand(BN, nodeLab);
        }

        return ActData_Utils::GetEntry(nodeLab);
    }

    //! Custom settle to ensure TagSource is present.
    void Settle(const TDF_Label& theLabel)
    {
        this->settleOn(theLabel);
        TDF_TagSource::Set(theLabel);
    }

protected:
    //! Default constructor.
    BrNode_Partition() : ActData_BasePartition() {}
};

#endif
