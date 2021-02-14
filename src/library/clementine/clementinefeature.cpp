#include "library/clementine/clementinefeature.h"

#include <QList>
#include <QMessageBox>
#include <QtDebug>

#include "library/baseexternalplaylistmodel.h"
#include "library/clementine/clementinedbconnection.h"
#include "library/clementine/clementineplaylistmodel.h"
#include "library/dao/settingsdao.h"
#include "library/library.h"
#include "track/track.h"

ClementineFeature::ClementineFeature(
        Library* pLibrary, UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig),
          m_connection(std::make_shared<ClementineDbConnection>(
                  m_pLibrary->trackCollections())),
          m_isActivated(false),
          m_pClementinePlaylistModel(make_parented<ClementinePlaylistModel>(
                  this, m_pLibrary->trackCollections(), m_connection)),
          m_childModel(),
          m_playlists(),
          m_future(),
          m_title(tr("Clementine")),
          m_icon(":/images/library/ic_library_clementine.svg") {
}

ClementineFeature::~ClementineFeature() {
    qDebug() << "~ClementineFeature()";
    if (m_future.isRunning()) {
        qDebug() << "m_future still running";
        m_future.waitForFinished();
        qDebug() << "m_future finished";
    }
}

// static
bool ClementineFeature::isSupported() {
    return !ClementineDbConnection::getDatabaseFile().isEmpty();
}

QVariant ClementineFeature::title() {
    return m_title;
}

QIcon ClementineFeature::getIcon() {
    return m_icon;
}

void ClementineFeature::activate() {
    qDebug() << "ClementineFeature::activate()";

    QString databaseFile = ClementineDbConnection::getDatabaseFile();

    if (!m_isActivated) {
        if (!QFile::exists(databaseFile)) {
            QMessageBox::warning(
                    nullptr,
                    tr("Error loading Clementine database"),
                    tr("Clementine database file not found at\n") +
                            databaseFile);
            qInfo() << "Clementine database file" << databaseFile << "does not exist";
            return;
        }

        if (!m_connection->open(databaseFile)) {
            QMessageBox::warning(
                    nullptr,
                    tr("Error loading Clementine database"),
                    tr("There was an error loading your Clementine database at\n") +
                            databaseFile);
            return;
        }
    }
    m_isActivated = true;

    std::unique_ptr<TreeItem> pRootItem = TreeItem::newRoot(this);
    QList<ClementinePlaylist> playlists = m_connection->getPlaylists();
    for (const ClementinePlaylist& playlist : playlists) {
        // append the playlist to the child model
        pRootItem->appendChild(playlist.name, playlist.playlistId);
    }

    m_childModel.setRootItem(std::move(pRootItem));
    qDebug() << "Clementine library loaded: success";

    //calls a slot in the sidebarmodel such that 'isLoading' is removed from the feature title.
    m_title = tr("Clementine");
    emit featureLoadingFinished(this);

    m_pClementinePlaylistModel->setTableModel(0); // Gets the master playlist
    emit showTrackModel(m_pClementinePlaylistModel);
    emit enableCoverArtDisplay(false);
}

void ClementineFeature::activateChild(const QModelIndex& index) {
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    int playlistID = item->getData().toInt();
    if (playlistID > 0) {
        qDebug() << "Activating " << item->getLabel();
        m_pClementinePlaylistModel->setTableModel(playlistID);
        emit showTrackModel(m_pClementinePlaylistModel);
        emit enableCoverArtDisplay(false);
    }
}

TreeItemModel* ClementineFeature::getChildModel() {
    return &m_childModel;
}

void ClementineFeature::appendTrackIdsFromRightClickIndex(
        QList<TrackId>* trackIds, QString* pPlaylist) {
    if (!lastRightClickedIndex().isValid()) {
        return;
    }

    TreeItem* item = static_cast<TreeItem*>(lastRightClickedIndex().internalPointer());
    *pPlaylist = item->getLabel();

    int playlistID = item->getData().toInt();
    if (playlistID <= 0) {
        return;
    }

    std::unique_ptr<ClementinePlaylistModel> pPlaylistModelToAdd =
            std::make_unique<ClementinePlaylistModel>(
                    this,
                    m_pLibrary->trackCollections(),
                    m_connection);
    pPlaylistModelToAdd->setTableModel(playlistID);
    pPlaylistModelToAdd->select();

    // Copy Tracks
    int rows = pPlaylistModelToAdd->rowCount();
    for (int i = 0; i < rows; ++i) {
        QModelIndex index = pPlaylistModelToAdd->index(i, 0);
        if (index.isValid()) {
            TrackPointer track = pPlaylistModelToAdd->getTrack(index);
            trackIds->append(track->getId());
        }
    }
}
