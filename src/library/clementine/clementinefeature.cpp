#include <QMessageBox>
#include <QtDebug>
#include <QList>

#include "library/clementine/clementinefeature.h"

#include "library/clementine/clementinedbconnection.h"
#include "library/dao/settingsdao.h"
#include "library/baseexternalplaylistmodel.h"
#include "library/clementine/clementineplaylistmodel.h"

const QString ClementineFeature::Clementine_MOUNT_KEY = "mixxx.ClementineFeature.mount";
QString ClementineFeature::m_databaseFile;

ClementineFeature::ClementineFeature(QObject* parent,
                               TrackCollection* pTrackCollection,
                               UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(parent, pTrackCollection),
          m_pTrackCollection(pTrackCollection),
          m_cancelImport(false) {
    Q_UNUSED(pConfig);
    m_pClementinePlaylistModel = new ClementinePlaylistModel(this, m_pTrackCollection, &m_connection);
    m_isActivated = false;
    m_title = tr("Clementine");
}

ClementineFeature::~ClementineFeature() {
    qDebug() << "~ClementineFeature()";
    // stop import thread, if still running
    m_cancelImport = true;
    if (m_future.isRunning()) {
        qDebug() << "m_future still running";
        m_future.waitForFinished();
        qDebug() << "m_future finished";
    }

    delete m_pClementinePlaylistModel;
}

// static
bool ClementineFeature::isSupported() {
    return !m_databaseFile.isEmpty();
}

// static
void ClementineFeature::prepareDbPath(UserSettingsPointer pConfig) {
    m_databaseFile = pConfig->getValueString(ConfigKey("[Clementine]","Database"));
    if (!QFile::exists(m_databaseFile)) {
        // Fall back to default
        m_databaseFile = ClementineDbConnection::getDatabaseFile();
    }
}

QVariant ClementineFeature::title() {
    return m_title;
}

QIcon ClementineFeature::getIcon() {
    return QIcon();//TODO ADD ICON ":/images/library/ic_library_clementine.png");
}

void ClementineFeature::activate() {
    qDebug("ClementineFeature::activate()");

    if (!m_isActivated) {
        if (!QFile::exists(m_databaseFile)) {
            // Fall back to default
            m_databaseFile = ClementineDbConnection::getDatabaseFile();
        }

        if (!QFile::exists(m_databaseFile)) {
            QMessageBox::warning(
                    NULL,
                    tr("Error loading Clementine database"),
                    tr("Clementine database file not found at\n") +
                    m_databaseFile);
            qDebug() << m_databaseFile << "does not exist";
        }

        if (!m_connection.open(m_databaseFile)) {
            QMessageBox::warning(
                    NULL,
                    tr("Error loading Clementine database"),
                    tr("There was an error loading your Clementine database at\n") +
                    m_databaseFile);
            return;
        }

        m_isActivated =  true;

        auto pRootItem = std::make_unique<TreeItem>(this);
        QList<ClementineDbConnection::Playlist> playlists = m_connection.getPlaylists();

        for (const ClementineDbConnection::Playlist& playlist: playlists) {
            qDebug() << playlist.name;
            // append the playlist to the child model
            pRootItem->appendChild(playlist.name, playlist.playlistId);
        }

        m_childModel.setRootItem(std::move(pRootItem));

        if (m_isActivated) {
            activate();
        }

        qDebug() << "Clementine library loaded: success";

        //calls a slot in the sidebarmodel such that 'isLoading' is removed from the feature title.
        m_title = tr("Clementine");
        emit(featureLoadingFinished(this));
    }

    m_pClementinePlaylistModel->setTableModel(0); // Gets the master playlist
    emit(showTrackModel(m_pClementinePlaylistModel));
    emit(enableCoverArtDisplay(false));
}

void ClementineFeature::activateChild(const QModelIndex& index) {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    int playlistID = item->getData().toInt();
    if (playlistID > 0) {
        qDebug() << "Activating " << item->getLabel();
        m_pClementinePlaylistModel->setTableModel(playlistID);
        emit(showTrackModel(m_pClementinePlaylistModel));
        emit(enableCoverArtDisplay(false));
    }
}

TreeItemModel* ClementineFeature::getChildModel() {
    return &m_childModel;
}

void ClementineFeature::appendTrackIdsFromRightClickIndex(QList<TrackId>* trackIds, QString* pPlaylist) {
    if (m_lastRightClickedIndex.isValid()) {
        TreeItem *item = static_cast<TreeItem*>(m_lastRightClickedIndex.internalPointer());
        *pPlaylist = item->getLabel();
        int playlistID = item->getData().toInt();
        qDebug() << "ClementineFeature::appendTrackIdsFromRightClickIndex " << *pPlaylist << " " << playlistID;
        if (playlistID > 0) {
            ClementinePlaylistModel* pPlaylistModelToAdd = new ClementinePlaylistModel(this, m_pTrackCollection, &m_connection);
            pPlaylistModelToAdd->setTableModel(playlistID);

            // Copy Tracks
            int rows = pPlaylistModelToAdd->rowCount();
            for (int i = 0; i < rows; ++i) {
                QModelIndex index = pPlaylistModelToAdd->index(i,0);
                if (index.isValid()) {
                    //qDebug() << pPlaylistModelToAdd->getTrackLocation(index);
                    TrackPointer track = pPlaylistModelToAdd->getTrack(index);
                    trackIds->append(track->getId());
                }
            }
            delete pPlaylistModelToAdd;
        }
    }
}
