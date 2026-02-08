#include <QApplication>
#include <QOpenGLWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <iostream>

// 简单测试OCCT头文件是否能正确包含
#include <gp_Pnt.hxx>
#include <BRepPrimAPI_MakeBox.hxx>

class SimpleWidget : public QOpenGLWidget
{
public:
    SimpleWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {}
    
protected:
    void initializeGL() override {
        std::cout << "OpenGL initialized successfully!" << std::endl;
    }
    
    void paintGL() override {
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    void resizeGL(int w, int h) override {
        glViewport(0, 0, w, h);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    std::cout << "Testing OCCT functionality..." << std::endl;
    
    // 测试OCCT基本功能
    gp_Pnt point(0, 0, 0);
    std::cout << "Created point: (" << point.X() << ", " << point.Y() << ", " << point.Z() << ")" << std::endl;
    
    try {
        // 创建一个简单的box
        BRepPrimAPI_MakeBox box(1.0, 1.0, 1.0);
        std::cout << "Created box successfully!" << std::endl;
    } catch (...) {
        std::cout << "Error creating box" << std::endl;
    }
    
    // 创建窗口
    QWidget window;
    window.setWindowTitle("OCCT-Qt Test");
    window.resize(800, 600);
    
    QVBoxLayout *layout = new QVBoxLayout(&window);
    SimpleWidget *glWidget = new SimpleWidget(&window);
    layout->addWidget(glWidget);
    
    window.show();
    
    std::cout << "Starting application event loop..." << std::endl;
    
    return app.exec();
}