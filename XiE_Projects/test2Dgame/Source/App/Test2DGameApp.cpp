#include "Test2DGameApp.h"

#include "Core/Log.h"

void Test2DGameApp::OnInit() {
    XLOG_INFO("Test2DGameApp initialized");
}

void Test2DGameApp::OnUpdate(float dt) {
    (void)dt;
}

void Test2DGameApp::OnRender() {}

void Test2DGameApp::OnShutdown() {
    XLOG_INFO("Test2DGameApp shutdown");
}

