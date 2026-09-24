#include "AssetServer.h"

#include "DefaultVertexShader.h"
#include "stdafx.h"

std::shared_ptr<FragmentShader> AssetServer::defaultFragShader = std::make_shared<BlinnPhongSIMD>();

std::shared_ptr<VertexShader> AssetServer::defaultVertShader =
        std::make_shared<DefaultVertexShader>();