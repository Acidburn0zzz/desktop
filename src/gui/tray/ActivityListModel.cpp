/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QtCore>
#include <QAbstractListModel>
#include <QWidget>
#include <QJsonObject>
#include <QJsonDocument>

#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "folderman.h"
#include "iconjob.h"
#include "accessmanager.h"

#include "ActivityData.h"
#include "ActivityListModel.h"

#include "theme.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcActivity, "nextcloud.gui.activity", QtInfoMsg)

ActivityListModel::ActivityListModel(AccountState *accountState, QObject *parent)
    : QAbstractListModel()
    , _accountState(accountState)
{
}

QHash<int, QByteArray> ActivityListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DisplayPathRole] = "displaypath";
    roles[PathRole] = "path";
    roles[LinkRole] = "link";
    roles[MessageRole] = "message";
    roles[ActionRole] = "type";
    roles[ActionIconRole] = "icon";
    roles[ActionTextRole] = "subject";
    roles[ActionTextColorRole] = "activityTextTitleColor";
    roles[ObjectTypeRole] = "objectType";
    roles[PointInTimeRole] = "dateTime";
    return roles;
}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    Activity a;

    if (!index.isValid())
        return QVariant();

    a = _finalList.at(index.row());
    AccountStatePtr ast = AccountManager::instance()->account(a._accName);
    if (!ast && _accountState != ast.data())
        return QVariant();
    QStringList list;

    switch (role) {
    case DisplayPathRole:
        if (!a._file.isEmpty()) {
            auto folder = FolderMan::instance()->folder(a._folder);
            QString relPath(a._file);
            if (folder) {
                relPath.prepend(folder->remotePath());
            }
            list = FolderMan::instance()->findFileInLocalFolders(relPath, ast->account());
            if (list.count() > 0) {
                if (relPath.startsWith('/') || relPath.startsWith('\\')) {
                    return relPath.remove(0, 1);
                } else {
                    return relPath;
                }
            }
        }
        return QString();
    case PathRole:
        if (!a._file.isEmpty()) {
            auto folder = FolderMan::instance()->folder(a._folder);
            QString relPath(a._file);
            if (folder)
                relPath.prepend(folder->remotePath());
            list = FolderMan::instance()->findFileInLocalFolders(relPath, ast->account());
            if (list.count() > 0) {
                QString path = "file:///" + QString(list.at(0));
                return QUrl(path);
            }
            // File does not exist anymore? Let's try to open its path
            if (QFileInfo(relPath).exists()) {
                list = FolderMan::instance()->findFileInLocalFolders(QFileInfo(relPath).path(), ast->account());
                if (list.count() > 0) {
                    return QVariant(list.at(0));
                }
            }
        }
        return QString();
    case ActionsLinksRole: {
        QList<QVariant> customList;
        foreach (ActivityLink customItem, a._links) {
            QVariant customVariant;
            customVariant.setValue(customItem);
            customList << customVariant;
        }
        return customList;
    }
    case ActionIconRole: {
        if (a._type == Activity::NotificationType) {
            return "qrc:///client/theme/black/bell.svg";
        } else if (a._type == Activity::SyncResultType) {
            return "qrc:///client/theme/black/state-error.svg";
        } else if (a._type == Activity::SyncFileItemType) {
            if (a._status == SyncFileItem::NormalError
                || a._status == SyncFileItem::FatalError
                || a._status == SyncFileItem::DetailError
                || a._status == SyncFileItem::BlacklistedError) {
                return "qrc:///client/theme/black/state-error.svg";
            } else if (a._status == SyncFileItem::SoftError
                || a._status == SyncFileItem::Conflict
                || a._status == SyncFileItem::Restoration
                || a._status == SyncFileItem::FileLocked) {
                return "qrc:///client/theme/black/state-warning.svg";
            } else if (a._status == SyncFileItem::FileIgnored) {
                return "qrc:///client/theme/black/state-info.svg";
            } else {
                // File sync successful
                if (a._fileAction == "file_created") {
                    return "qrc:///client/theme/colored/add.svg";
                } else if (a._fileAction == "file_deleted") {
                    return "qrc:///client/theme/colored/delete.svg";
                } else {
                    return "qrc:///client/theme/change.svg";
                }
            }
        } else {
            // We have an activity
            if (!a._iconData.isEmpty()) {
                QString svgData = "data:image/svg+xml;utf8," + a._iconData;
                return svgData;
            }
            return "qrc:///client/theme/black/activity.svg";
        }
    }
    case ObjectTypeRole:
        return a._objectType;
    case ActionRole: {
        switch (a._type) {
        case Activity::ActivityType:
            return "Activity";
        case Activity::NotificationType:
            return "Notification";
        case Activity::SyncFileItemType:
            return "File";
        case Activity::SyncResultType:
            return "Sync";
        default:
            return QVariant();
        }
    }
    case ActionTextRole:
        return a._subject;
    case ActionTextColorRole:
        return a._id == -1 ? QLatin1String("#808080") : QLatin1String("#222");   // FIXME: This is a temporary workaround for _showMoreActivitiesAvailableEntry
    case MessageRole:
        if (a._message.isEmpty()) {
            return QString("No description available.");
        }
        return a._message;
    case LinkRole: {
        if (a._link.isEmpty()) {
            return "";
        } else {
            return a._link;
        }
    }
    case AccountRole:
        return a._accName;
    case PointInTimeRole:
        //return a._id == -1 ? "" : QString("%1 - %2").arg(Utility::timeAgoInWords(a._dateTime.toLocalTime()), a._dateTime.toLocalTime().toString(Qt::DefaultLocaleShortDate));
        return a._id == -1 ? "" : Utility::timeAgoInWords(a._dateTime.toLocalTime());
    case AccountConnectedRole:
        return (ast && ast->isConnected());
    default:
        return QVariant();
    }
    return QVariant();
}

int ActivityListModel::rowCount(const QModelIndex &) const
{
    return _finalList.count();
}

bool ActivityListModel::canFetchMore(const QModelIndex &) const
{
    // We need to be connected to be able to fetch more
    if (_accountState && _accountState->isConnected()) {
        // If the fetching is reported to be done or we are currently fetching we can't fetch more
        if (!_doneFetching && !_currentlyFetching) {
            return true;
        }
    }

    return false;
}

void ActivityListModel::startFetchJob()
{
    if (!_accountState->isConnected()) {
        return;
    }
    JsonApiJob *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/apps/activity/api/v2/activity"), this);
    QObject::connect(job, &JsonApiJob::jsonReceived,
        this, &ActivityListModel::slotActivitiesReceived);

    QUrlQuery params;
    params.addQueryItem(QLatin1String("since"), QString::number(_currentItem));
    params.addQueryItem(QLatin1String("limit"), QString::number(50));
    job->addQueryParams(params);

    _currentlyFetching = true;
    qCInfo(lcActivity) << "Start fetching activities for " << _accountState->account()->displayName();
    job->start();
}

void ActivityListModel::slotActivitiesReceived(const QJsonDocument &json, int statusCode)
{
    auto activities = json.object().value("ocs").toObject().value("data").toArray();

    ActivityList list;
    auto ast = _accountState;
    if (!ast) {
        return;
    }

    if (activities.size() == 0) {
        _doneFetching = true;
    }

    _currentlyFetching = false;

    QDateTime oldestDate = QDateTime::currentDateTime();
    oldestDate = oldestDate.addDays(_maxActivitiesDays * -1);

    foreach (auto activ, activities) {
        auto json = activ.toObject();

        Activity a;
        a._type = Activity::ActivityType;
        a._objectType = json.value("object_type").toString();
        a._accName = ast->account()->displayName();
        a._id = json.value("activity_id").toInt();
        a._fileAction = json.value("type").toString();
        a._subject = json.value("subject").toString();
        a._message = json.value("message").toString();
        a._file = json.value("object_name").toString();
        a._link = QUrl(json.value("link").toString());
        a._dateTime = QDateTime::fromString(json.value("datetime").toString(), Qt::ISODate);
        a._icon = json.value("icon").toString();

        if (!a._icon.isEmpty()) {
            IconJob *iconJob = new IconJob(QUrl(a._icon));
            iconJob->setProperty("activityId", a._id);
            connect(iconJob, &IconJob::jobFinished, this, &ActivityListModel::slotIconDownloaded);
        }

        list.append(a);
        _currentItem = list.last()._id;

        _totalActivitiesFetched++;
        if(_totalActivitiesFetched == _maxActivities ||
            a._dateTime < oldestDate) {

            _showMoreActivitiesAvailableEntry = true;
            _doneFetching = true;
            break;
        }
    }

    _activityLists.append(list);

    emit activityJobStatusCode(statusCode);

    combineActivityLists();
}

void ActivityListModel::slotIconDownloaded(QByteArray iconData)
{
    for (size_t i = 0; i < _activityLists.count(); i++) {
        if (_activityLists[i]._id == sender()->property("activityId").toLongLong()) {
            _activityLists[i]._iconData = iconData;
        }
    }
}

void ActivityListModel::addErrorToActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Error successfully added to the notification list: " << activity._subject;
    _notificationErrorsLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::addIgnoredFileToList(Activity newActivity)
{
    qCInfo(lcActivity) << "First checking for duplicates then add file to the notification list of ignored files: " << newActivity._file;

    bool duplicate = false;
    if (_listOfIgnoredFiles.size() == 0) {
        _notificationIgnoredFiles = newActivity;
        _notificationIgnoredFiles._subject = tr("Files from the ignore list as well as symbolic links are not synced. This includes:");
        _listOfIgnoredFiles.append(newActivity);
        return;
    }

    foreach (Activity activity, _listOfIgnoredFiles) {
        if (activity._file == newActivity._file) {
            duplicate = true;
            break;
        }
    }

    if (!duplicate) {
        _notificationIgnoredFiles._message.append(", " + newActivity._file);
    }
}

void ActivityListModel::addNotificationToActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Notification successfully added to the notification list: " << activity._subject;
    _notificationLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::clearNotifications()
{
    qCInfo(lcActivity) << "Clear the notifications";
    _notificationLists.clear();
    combineActivityLists();
}

void ActivityListModel::removeActivityFromActivityList(int row)
{
    Activity activity = _finalList.at(row);
    removeActivityFromActivityList(activity);
    combineActivityLists();
}

void ActivityListModel::addSyncFileItemToActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Successfully added to the activity list: " << activity._subject;
    _syncFileItemLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::removeActivityFromActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Activity/Notification/Error successfully dismissed: " << activity._subject;
    qCInfo(lcActivity) << "Trying to remove Activity/Notification/Error from view... ";

    int index = -1;
    if (activity._type == Activity::ActivityType) {
        index = _activityLists.indexOf(activity);
        if (index != -1)
            _activityLists.removeAt(index);
    } else if (activity._type == Activity::NotificationType) {
        index = _notificationLists.indexOf(activity);
        if (index != -1)
            _notificationLists.removeAt(index);
    } else {
        index = _notificationErrorsLists.indexOf(activity);
        if (index != -1)
            _notificationErrorsLists.removeAt(index);
    }

    if (index != -1) {
        qCInfo(lcActivity) << "Activity/Notification/Error successfully removed from the list.";
        qCInfo(lcActivity) << "Updating Activity/Notification/Error view.";
        combineActivityLists();
    }
}

void ActivityListModel::combineActivityLists()
{
    ActivityList resultList;

    if (_notificationErrorsLists.count() > 0) {
        std::sort(_notificationErrorsLists.begin(), _notificationErrorsLists.end());
        resultList.append(_notificationErrorsLists);
    }
    if (_listOfIgnoredFiles.size() > 0)
        resultList.append(_notificationIgnoredFiles);

    if (_notificationLists.count() > 0) {
        std::sort(_notificationLists.begin(), _notificationLists.end());
        resultList.append(_notificationLists);
    }

    if (_syncFileItemLists.count() > 0) {
        std::sort(_syncFileItemLists.begin(), _syncFileItemLists.end());
        resultList.append(_syncFileItemLists);
    }

    if (_activityLists.count() > 0) {
        std::sort(_activityLists.begin(), _activityLists.end());
        resultList.append(_activityLists);

        if(_showMoreActivitiesAvailableEntry) {
            Activity a;
            a._type = Activity::ActivityType;
            a._accName = _accountState->account()->displayName();
            a._id = -1;
            a._subject = tr("For more activities please open the Activity app.");
            a._dateTime = QDateTime::currentDateTime();

            AccountApp *app = _accountState->findApp(QLatin1String("activity"));
            if(app) {
                a._link = app->url();
            }

            resultList.append(a);
        }
    }

    beginResetModel();
    _finalList.clear();
    endResetModel();

    if (resultList.count() > 0) {
        beginInsertRows(QModelIndex(), 0, resultList.count() - 1);
        _finalList = resultList;
        endInsertRows();
    }
}

bool ActivityListModel::canFetchActivities() const
{
    return _accountState->isConnected() && _accountState->account()->capabilities().hasActivities();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    if (canFetchActivities()) {
        startFetchJob();
    } else {
        _doneFetching = true;
        combineActivityLists();
    }
}

void ActivityListModel::slotRefreshActivity()
{
    _activityLists.clear();
    _doneFetching = false;
    _currentItem = 0;
    _totalActivitiesFetched = 0;
    _showMoreActivitiesAvailableEntry = false;

    if (canFetchActivities()) {
        startFetchJob();
    } else {
        _doneFetching = true;
        combineActivityLists();
    }
}

void ActivityListModel::slotRemoveAccount()
{
    _finalList.clear();
    _activityLists.clear();
    _currentlyFetching = false;
    _doneFetching = false;
    _currentItem = 0;
    _totalActivitiesFetched = 0;
    _showMoreActivitiesAvailableEntry = false;
}
}
