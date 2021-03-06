#include "library/trackcollection.h"
#include "widget/wanalysislibrarytableview.h"

WAnalysisLibraryTableView::WAnalysisLibraryTableView(QWidget* parent,
                                                   ConfigObject<ConfigValue>* pConfig,
                                                   TrackCollection* pTrackCollection)
        : WTrackTableView(parent, pConfig, pTrackCollection) {
    setDragDropMode(QAbstractItemView::DragOnly);
    setDragEnabled(true); //Always enable drag for now (until we have a model that doesn't support this.)
}

WAnalysisLibraryTableView::~WAnalysisLibraryTableView() {
}

void WAnalysisLibraryTableView::onSearchStarting() {
}

void WAnalysisLibraryTableView::onSearchCleared() {
}

void WAnalysisLibraryTableView::onSearch(const QString& text) {
    Q_UNUSED(text);
}

