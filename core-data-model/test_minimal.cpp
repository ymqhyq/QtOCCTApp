#include <iostream>
#include "generated/DataModel.h"

int main() {
    std::cout << "Starting minimal test..." << std::endl;
    try {
        Handle(DataModel) model = new DataModel();
        model->NewEmpty();
        std::cout << "Model created and initialized!" << std::endl;
        
        if (!model->GetRootNode().IsNull()) {
            std::cout << "Root node exists: " << TCollection_AsciiString(model->GetRootNode()->GetName()).ToCString() << std::endl;
        }
    } catch (Standard_Failure& e) {
        std::cerr << "OCCT Exception: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "Unknown Exception" << std::endl;
    }
    std::cout << "Minimal test finished." << std::endl;
    return 0;
}
