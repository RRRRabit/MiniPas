#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    // Qt GUI 程序入口。
    // QApplication 负责管理事件循环、窗口消息、鼠标键盘输入等。
    QApplication app(argc, argv);

    // 创建主窗口并显示。
    // 编译器真正的逻辑不在这里，而在 CompilerFacade 和核心模块中。
    MainWindow window;
    window.show();

    // app.exec() 会进入 Qt 事件循环。
    // 用户点击按钮、切换表格、输入代码，都是由这个事件循环分发处理。
    return app.exec();
}
