#pragma once

#if ENABLE(ASYNC_SCROLLING) && ENABLE(UI_SIDE_COMPOSITING)

#include "RemoteScrollingCoordinatorTransaction.h"

#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/ScrollingStateFrameScrollingNode.h>
#include <WebCore/ScrollingStateTree.h>
#include <wtf/Assertions.h>

namespace WebKit {

struct NutjobScrollingTransactionSummary {
    bool hasStateTree { false };
    bool hasChangedProperties { false };
    bool hasNewRootStateNode { false };
    bool clearScrollLatching { false };
    unsigned nodeCount { 0 };
    unsigned scrollingNodeCount { 0 };
    unsigned long long frameID { 0 };
    unsigned long long rootNodeID { 0 };
    unsigned long long rootChangedProperties { 0 };
    float scrollPositionX { 0 };
    float scrollPositionY { 0 };
    int scrollOriginX { 0 };
    int scrollOriginY { 0 };
    float layoutViewportX { 0 };
    float layoutViewportY { 0 };
    float layoutViewportWidth { 0 };
    float layoutViewportHeight { 0 };
    float visibleContentWidth { 0 };
    float visibleContentHeight { 0 };
    unsigned requestedScrollCount { 0 };
    unsigned long long scrollContainerLayerID { 0 };
    unsigned long long scrolledContentsLayerID { 0 };
    unsigned long long rootContentsLayerID { 0 };
};

static inline bool nutjobScrollBreadcrumbsEnabled()
{
    static bool enabled = [] {
        if (const char* value = getenv("NUTJOB_COMPOSITOR_SCROLL_BREADCRUMBS"))
            return value[0] && value[0] != '0';
        return false;
    }();
    return enabled;
}

static inline unsigned long long nutjobFrameIDValue(std::optional<WebCore::FrameIdentifier> frameID)
{
    return frameID ? static_cast<unsigned long long>(frameID->toUInt64()) : 0;
}

static inline unsigned long long nutjobScrollingNodeIDValue(std::optional<WebCore::ScrollingNodeID> nodeID)
{
    return nodeID ? static_cast<unsigned long long>(nodeID->object().toUInt64()) : 0;
}

static inline unsigned long long nutjobPlatformLayerIDValue(const WebCore::LayerRepresentation& layerRepresentation)
{
    auto layerID = layerRepresentation.layerID();
    return layerID ? static_cast<unsigned long long>(layerID->object().toUInt64()) : 0;
}

static inline NutjobScrollingTransactionSummary nutjobSummarizeScrollingTransaction(const RemoteScrollingCoordinatorTransaction& transaction, std::optional<WebCore::FrameIdentifier> frameIDOverride = std::nullopt)
{
    NutjobScrollingTransactionSummary summary;
    summary.clearScrollLatching = transaction.clearScrollLatching();
    summary.frameID = frameIDOverride ? nutjobFrameIDValue(frameIDOverride) : nutjobFrameIDValue(transaction.rootFrameIdentifier());

    const auto& stateTree = transaction.scrollingStateTree();
    if (!stateTree)
        return summary;

    summary.hasStateTree = true;
    summary.hasChangedProperties = stateTree->hasChangedProperties();
    summary.hasNewRootStateNode = stateTree->hasNewRootStateNode();
    summary.nodeCount = stateTree->nodeCount();
    summary.scrollingNodeCount = stateTree->scrollingNodeCount();

    RefPtr rootStateNode = stateTree->rootStateNode();
    if (!rootStateNode)
        return summary;

    summary.rootNodeID = static_cast<unsigned long long>(rootStateNode->scrollingNodeID().object().toUInt64());
    summary.rootChangedProperties = static_cast<unsigned long long>(rootStateNode->changedProperties().toRaw());
    summary.scrollPositionX = rootStateNode->scrollPosition().x();
    summary.scrollPositionY = rootStateNode->scrollPosition().y();
    summary.scrollOriginX = rootStateNode->scrollOrigin().x();
    summary.scrollOriginY = rootStateNode->scrollOrigin().y();
    summary.layoutViewportX = rootStateNode->layoutViewport().x();
    summary.layoutViewportY = rootStateNode->layoutViewport().y();
    summary.layoutViewportWidth = rootStateNode->layoutViewport().width();
    summary.layoutViewportHeight = rootStateNode->layoutViewport().height();
    summary.visibleContentWidth = rootStateNode->sizeForVisibleContent().width();
    summary.visibleContentHeight = rootStateNode->sizeForVisibleContent().height();
    summary.requestedScrollCount = rootStateNode->requestedScrollData().size();
    summary.scrollContainerLayerID = nutjobPlatformLayerIDValue(rootStateNode->scrollContainerLayer());
    summary.scrolledContentsLayerID = nutjobPlatformLayerIDValue(rootStateNode->scrolledContentsLayer());
    summary.rootContentsLayerID = nutjobPlatformLayerIDValue(rootStateNode->rootContentsLayer());
    return summary;
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(UI_SIDE_COMPOSITING)
