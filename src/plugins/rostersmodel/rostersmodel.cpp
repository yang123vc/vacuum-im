#include "rostersmodel.h"

#include <QTimer>

#define INDEX_CHANGES_FOR_RESET 20

RostersModel::RostersModel()
{
	FRosterPlugin = NULL;
	FPresencePlugin = NULL;
	FAccountManager = NULL;

	FRootIndex = new RosterIndex(RIT_ROOT,"IT_Root");
	FRootIndex->setParent(this);
	connect(FRootIndex,SIGNAL(dataChanged(IRosterIndex *, int)),
	        SLOT(onIndexDataChanged(IRosterIndex *, int)));
	connect(FRootIndex,SIGNAL(childAboutToBeInserted(IRosterIndex *)),
	        SLOT(onIndexChildAboutToBeInserted(IRosterIndex *)));
	connect(FRootIndex,SIGNAL(childInserted(IRosterIndex *)),
	        SLOT(onIndexChildInserted(IRosterIndex *)));
	connect(FRootIndex,SIGNAL(childAboutToBeRemoved(IRosterIndex *)),
	        SLOT(onIndexChildAboutToBeRemoved(IRosterIndex *)));
	connect(FRootIndex,SIGNAL(childRemoved(IRosterIndex *)),
	        SLOT(onIndexChildRemoved(IRosterIndex *)));
}

RostersModel::~RostersModel()
{

}

void RostersModel::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Roster Model");
	APluginInfo->description = tr("Creates a hierarchical model for display roster");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://www.vacuum-im.org";
}

bool RostersModel::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
	IPlugin *plugin = APluginManager->pluginInterface("IRosterPlugin").value(0,NULL);
	if (plugin)
	{
		FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
		if (FRosterPlugin)
		{
			connect(FRosterPlugin->instance(),SIGNAL(rosterItemReceived(IRoster *, const IRosterItem &)),
			        SLOT(onRosterItemReceived(IRoster *, const IRosterItem &)));
			connect(FRosterPlugin->instance(),SIGNAL(rosterItemRemoved(IRoster *, const IRosterItem &)),
			        SLOT(onRosterItemRemoved(IRoster *, const IRosterItem &)));
			connect(FRosterPlugin->instance(),SIGNAL(rosterStreamJidChanged(IRoster *, const Jid &)),
			        SLOT(onRosterStreamJidChanged(IRoster *, const Jid &)));
		}
	}

	plugin = APluginManager->pluginInterface("IPresencePlugin").value(0,NULL);
	if (plugin)
	{
		FPresencePlugin = qobject_cast<IPresencePlugin *>(plugin->instance());
		if (FPresencePlugin)
		{
			connect(FPresencePlugin->instance(),SIGNAL(presenceChanged(IPresence *, int, const QString &, int)),
			        SLOT(onPresenceChanged(IPresence *, int , const QString &, int)));
			connect(FPresencePlugin->instance(),SIGNAL(presenceReceived(IPresence *, const IPresenceItem &)),
			        SLOT(onPresenceReceived(IPresence *, const IPresenceItem &)));
		}
	}

	plugin = APluginManager->pluginInterface("IAccountManager").value(0,NULL);
	if (plugin)
	{
		FAccountManager = qobject_cast<IAccountManager *>(plugin->instance());
		if (FAccountManager)
		{
			connect(FAccountManager->instance(),SIGNAL(shown(IAccount *)),SLOT(onAccountShown(IAccount *)));
			connect(FAccountManager->instance(),SIGNAL(hidden(IAccount *)),SLOT(onAccountHidden(IAccount *)));
		}
	}

	return true;
}

QModelIndex RostersModel::index(int ARow, int AColumn, const QModelIndex &AParent) const
{
	IRosterIndex *parentIndex = AParent.isValid() ? reinterpret_cast<IRosterIndex *>(AParent.internalPointer()) : FRootIndex;

	IRosterIndex *childIndex = parentIndex->child(ARow);
	if (childIndex)
		return createIndex(ARow,AColumn,childIndex);

	return QModelIndex();
}

QModelIndex RostersModel::parent(const QModelIndex &AIndex) const
{
	if (AIndex.isValid())
		return modelIndexByRosterIndex(reinterpret_cast<IRosterIndex *>(AIndex.internalPointer())->parentIndex());
	return QModelIndex();
}

bool RostersModel::hasChildren(const QModelIndex &AParent) const
{
	IRosterIndex *parentIndex = AParent.isValid() ? reinterpret_cast<IRosterIndex *>(AParent.internalPointer()) : FRootIndex;
	return parentIndex->childCount() > 0;
}

int RostersModel::rowCount(const QModelIndex &AParent) const
{
	IRosterIndex *parentIndex = AParent.isValid() ? reinterpret_cast<IRosterIndex *>(AParent.internalPointer()) : FRootIndex;
	return parentIndex->childCount();
}

int RostersModel::columnCount(const QModelIndex &AParent) const
{
	Q_UNUSED(AParent);
	return 1;
}

Qt::ItemFlags RostersModel::flags(const QModelIndex &AIndex) const
{
	IRosterIndex *rosterIndex = AIndex.isValid() ? reinterpret_cast<IRosterIndex *>(AIndex.internalPointer()) : FRootIndex;
	return rosterIndex->flags();
}

QVariant RostersModel::data(const QModelIndex &AIndex, int ARole) const
{
	IRosterIndex *index = AIndex.isValid() ? reinterpret_cast<IRosterIndex *>(AIndex.internalPointer()) : FRootIndex;
	return index->data(ARole);
}

QMap<int, QVariant> RostersModel::itemData(const QModelIndex &AIndex) const
{
	IRosterIndex *index = AIndex.isValid() ? reinterpret_cast<IRosterIndex *>(AIndex.internalPointer()) : FRootIndex;
	return index->data();
}

IRosterIndex *RostersModel::addStream(const Jid &AStreamJid)
{
	IRosterIndex *streamIndex = FStreamsRoot.value(AStreamJid);
	if (streamIndex == NULL)
	{
		IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
		IPresence *presence = FPresencePlugin!=NULL ? FPresencePlugin->getPresence(AStreamJid) : NULL;
		IAccount *account = FAccountManager!=NULL ? FAccountManager->accountByStream(AStreamJid) : NULL;

		if (roster || presence)
		{
			IRosterIndex *streamIndex = createRosterIndex(RIT_STREAM_ROOT,AStreamJid.pFull(),FRootIndex);
			streamIndex->setRemoveOnLastChildRemoved(false);
			streamIndex->setData(RDR_STREAM_JID,AStreamJid.pFull());
			streamIndex->setData(RDR_JID,AStreamJid.full());
			streamIndex->setData(RDR_PJID,AStreamJid.pFull());
			streamIndex->setData(RDR_BARE_JID,AStreamJid.pBare());

			if (presence)
			{
				streamIndex->setData(RDR_SHOW, presence->show());
				streamIndex->setData(RDR_STATUS,presence->status());
			}
			if (account)
			{
				streamIndex->setData(RDR_NAME,account->name());
				connect(account->instance(),SIGNAL(optionsChanged(const OptionsNode &)),SLOT(onAccountOptionsChanged(const OptionsNode &)));
			}

			FStreamsRoot.insert(AStreamJid,streamIndex);
			insertRosterIndex(streamIndex,FRootIndex);
			emit streamAdded(AStreamJid);

			if (roster)
			{
				foreach(IRosterItem item, roster->rosterItems())
				onRosterItemReceived(roster,item);
			}
		}
	}
	return streamIndex;
}

void RostersModel::removeStream(const Jid &AStreamJid)
{
	IRosterIndex *streamIndex = FStreamsRoot.take(AStreamJid);
	if (streamIndex)
	{
		IAccount *account = FAccountManager!=NULL ? FAccountManager->accountByStream(AStreamJid) : NULL;
		if (account)
		{
			connect(account->instance(),SIGNAL(optionsChanged(const OptionsNode &)),this,SLOT(onAccountOptionsChanged(const OptionsNode &)));
		}
		removeRosterIndex(streamIndex);
		emit streamRemoved(AStreamJid);
	}
}

IRosterIndex *RostersModel::streamRoot(const Jid &AStreamJid) const
{
	return FStreamsRoot.value(AStreamJid);
}

IRosterIndex *RostersModel::createRosterIndex(int AType, const QString &AId, IRosterIndex *AParent)
{
	IRosterIndex *index = findRosterIndex(AType,AId,AParent);
	if (!index)
	{
		index = new RosterIndex(AType,AId);
		connect(index->instance(),SIGNAL(indexDestroyed(IRosterIndex *)),SLOT(onIndexDestroyed(IRosterIndex *)));
		if (AParent)
			index->setData(RDR_STREAM_JID,AParent->data(RDR_STREAM_JID));
		emit indexCreated(index,AParent);
		insertDefaultDataHolders(index);
	}
	return index;
}

IRosterIndex *RostersModel::createGroup(const QString &AName, const QString &AGroupDelim, int AType, IRosterIndex *AParent)
{
	IRosterIndex *index = findGroup(AName,AGroupDelim,AType,AParent);
	if (!index)
	{
		int i = 0;
		index = AParent;
		IRosterIndex *newIndex = (IRosterIndex *)true;
		QList<QString> groupTree = AName.split(AGroupDelim,QString::SkipEmptyParts);

		QString group;
		if (AParent && AParent->data(RDR_GROUP).isValid())
			group = AParent->data(RDR_GROUP).toString();

		while (newIndex && i<groupTree.count())
		{
			if (group.isEmpty())
				group = groupTree.at(i);
			else
				group += AGroupDelim + groupTree.at(i);

			newIndex = findGroup(groupTree.at(i),AGroupDelim,AType,index);
			if (newIndex)
			{
				index = newIndex;
				i++;
			}
		}

		while (i<groupTree.count())
		{
			newIndex = createRosterIndex(AType,groupTree.at(i),index);
			newIndex->setData(RDR_GROUP,group);
			newIndex->setData(RDR_NAME,groupTree.at(i));
			insertRosterIndex(newIndex,index);
			index = newIndex;
			group += AGroupDelim + groupTree.value(++i);
		}
	}
	return index;
}

void RostersModel::insertRosterIndex(IRosterIndex *AIndex, IRosterIndex *AParent)
{
	AIndex->setParentIndex(AParent);
}

void RostersModel::removeRosterIndex(IRosterIndex *AIndex)
{
	AIndex->setParentIndex(NULL);
}

QList<IRosterIndex *> RostersModel::getContactIndexList(const Jid &AStreamJid, const Jid &AContactJid, bool ACreate)
{
	QList<IRosterIndex *> indexList;
	IRosterIndex *streamIndex = FStreamsRoot.value(AStreamJid);
	if (streamIndex)
	{
		int type = RIT_CONTACT;
		if (AContactJid.node().isEmpty())
			type = RIT_AGENT;
		else if (AContactJid && AStreamJid)
			type = RIT_MY_RESOURCE;

		QHash<int,QVariant> data;
		data.insert(RDR_TYPE, type);
		if (AContactJid.resource().isEmpty())
			data.insert(RDR_BARE_JID, AContactJid.pBare());
		else
			data.insert(RDR_PJID, AContactJid.pFull());
		indexList = streamIndex->findChild(data,true);

		if (indexList.isEmpty() && !AContactJid.resource().isEmpty())
		{
			data.insert(RDR_PJID, AContactJid.pBare());
			indexList = streamIndex->findChild(data,true);
		}

		if (indexList.isEmpty() && ACreate)
		{
			IRosterIndex *group;
			if (type == RIT_MY_RESOURCE)
				group = createGroup(myResourcesGroupName(),"::",RIT_GROUP_MY_RESOURCES,streamIndex);
			else
				group = createGroup(notInRosterGroupName(),"::",RIT_GROUP_NOT_IN_ROSTER,streamIndex);
			IRosterIndex *index = createRosterIndex(type,AContactJid.pFull(),group);
			index->setData(RDR_JID,AContactJid.full());
			index->setData(RDR_PJID,AContactJid.pFull());
			index->setData(RDR_BARE_JID,AContactJid.pBare());
			index->setData(RDR_GROUP,group->data(RDR_GROUP));
			insertRosterIndex(index,group);
			indexList.append(index);
		}
	}
	return indexList;
}

IRosterIndex *RostersModel::findRosterIndex(int AType, const QString &AId, IRosterIndex *AParent) const
{
	QHash<int,QVariant> data;
	data.insert(RDR_TYPE,AType);
	data.insert(RDR_INDEX_ID,AId);
	if (AParent)
		return AParent->findChild(data).value(0,NULL);
	return FRootIndex->findChild(data).value(0,NULL) ;
}

IRosterIndex *RostersModel::findGroup(const QString &AName, const QString &AGroupDelim,
                                      int AType, IRosterIndex *AParent) const
{
	int i = 0;
	IRosterIndex *index = AParent;
	QList<QString> groupTree = AName.split(AGroupDelim,QString::SkipEmptyParts);
	do {
		index = findRosterIndex(AType,groupTree.at(i++),index);
	} while (index && i<groupTree.count());
	return index;
}

void RostersModel::insertDefaultDataHolder(IRosterDataHolder *ADataHolder)
{
	if (ADataHolder && !FDataHolders.contains(ADataHolder))
	{
		QMultiHash<int,QVariant> data;
		foreach(int type, ADataHolder->rosterDataTypes())
		data.insertMulti(RDR_TYPE,type);

		QList<IRosterIndex *> indexes = FRootIndex->findChild(data,true);
		foreach(IRosterIndex *index,indexes)
		index->insertDataHolder(ADataHolder);

		FDataHolders.append(ADataHolder);
		emit defaultDataHolderInserted(ADataHolder);
	}
}

void RostersModel::removeDefaultDataHolder(IRosterDataHolder *ADataHolder)
{
	if (ADataHolder && FDataHolders.contains(ADataHolder))
	{
		QMultiHash<int,QVariant> data;
		foreach(int type, ADataHolder->rosterDataTypes())
		data.insertMulti(RDR_TYPE,type);

		QList<IRosterIndex *> indexes = FRootIndex->findChild(data,true);
		foreach(IRosterIndex *index, indexes)
		index->removeDataHolder(ADataHolder);

		FDataHolders.removeAt(FDataHolders.indexOf(ADataHolder));
		emit defaultDataHolderRemoved(ADataHolder);
	}
}

QModelIndex RostersModel::modelIndexByRosterIndex(IRosterIndex *AIndex) const
{
	return AIndex!=NULL && AIndex!=FRootIndex ? createIndex(AIndex->row(),0,AIndex) : QModelIndex();
}

IRosterIndex *RostersModel::rosterIndexByModelIndex(const QModelIndex &AIndex) const
{
	return reinterpret_cast<IRosterIndex *>(AIndex.internalPointer());
}

void RostersModel::emitDelayedDataChanged(IRosterIndex *AIndex)
{
	FChangedIndexes -= AIndex;

	if (AIndex!=FRootIndex)
	{
		QModelIndex modelIndex = modelIndexByRosterIndex(AIndex);
		emit dataChanged(modelIndex,modelIndex);
	}

	QList<IRosterIndex *> childs;
	foreach(IRosterIndex *index, FChangedIndexes)
	if (index->parentIndex() == AIndex)
		childs.append(index);

	foreach(IRosterIndex *index, childs)
	emitDelayedDataChanged(index);
}

void RostersModel::insertDefaultDataHolders(IRosterIndex *AIndex)
{
	foreach(IRosterDataHolder *dataHolder, FDataHolders)
	if (dataHolder->rosterDataTypes().contains(AIndex->type()))
		AIndex->insertDataHolder(dataHolder);
}

void RostersModel::onAccountShown(IAccount *AAccount)
{
	if (AAccount->isActive())
		addStream(AAccount->xmppStream()->streamJid());
}

void RostersModel::onAccountHidden(IAccount *AAccount)
{
	if (AAccount->isActive())
		removeStream(AAccount->xmppStream()->streamJid());
}

void RostersModel::onAccountOptionsChanged(const OptionsNode &ANode)
{
	IAccount *account = qobject_cast<IAccount *>(sender());
	if (account && account->isActive() && account->optionsNode().childPath(ANode)=="name")
	{
		IRosterIndex *streamIndex = FStreamsRoot.value(account->xmppStream()->streamJid());
		if (streamIndex)
			streamIndex->setData(RDR_NAME,account->name());
	}
}

void RostersModel::onRosterItemReceived(IRoster *ARoster, const IRosterItem &ARosterItem)
{
	IRosterIndex *streamIndex = FStreamsRoot.value(ARoster->streamJid());
	if (streamIndex)
	{
		int groupType;
		QString groupDisplay;
		QSet<QString> itemGroups;
		int itemType = !ARosterItem.itemJid.node().isEmpty() ? RIT_CONTACT : RIT_AGENT;

		if (itemType == RIT_AGENT)
		{
			groupType = RIT_GROUP_AGENTS;
			groupDisplay = agentsGroupName();
			itemGroups.insert("");
		}
		else if (ARosterItem.groups.isEmpty())
		{
			groupType = RIT_GROUP_BLANK;
			groupDisplay = blankGroupName();
			itemGroups.insert("");
		}
		else
		{
			groupType = RIT_GROUP;
			groupDisplay = blankGroupName();
			itemGroups = ARosterItem.groups;
		}

		QHash<int,QVariant> findData;
		findData.insert(RDR_TYPE,itemType);
		findData.insert(RDR_BARE_JID,ARosterItem.itemJid.pBare());
		QList<IRosterIndex *> curItemList = streamIndex->findChild(findData,true);

		QSet<QString> curGroups;
		foreach(IRosterIndex *index, curItemList)
		curGroups.insert(index->data(RDR_GROUP).toString());
		QSet<QString> newGroups = itemGroups - curGroups;
		QSet<QString> oldGroups = curGroups - itemGroups;

		QList<IRosterIndex *> itemList;
		QString groupDelim = ARoster->groupDelimiter();
		IPresence *presence = FPresencePlugin!=NULL ? FPresencePlugin->getPresence(ARoster->streamJid()) : NULL;
		foreach(QString group,itemGroups)
		{
			IRosterIndex *groupIndex = createGroup(!group.isEmpty() ? group : groupDisplay,groupDelim,groupType,streamIndex);

			QList<IRosterIndex *> groupItemList;
			if (newGroups.contains(group) && !oldGroups.isEmpty())
			{
				IRosterIndex *oldGroupIndex;
				QString oldGroup = oldGroups.values().value(0);
				if (oldGroup.isEmpty())
					oldGroupIndex = findGroup(blankGroupName(),groupDelim,RIT_GROUP_BLANK,streamIndex);
				else
					oldGroupIndex = findGroup(oldGroup,groupDelim,RIT_GROUP,streamIndex);
				if (oldGroupIndex)
				{
					groupItemList = oldGroupIndex->findChild(findData);
					foreach(IRosterIndex *index,groupItemList)
					{
						index->setData(RDR_GROUP,group);
						index->setParentIndex(groupIndex);
					}
				}
			}
			else
				groupItemList = groupIndex->findChild(findData);

			if (groupItemList.isEmpty())
			{
				int presIndex = 0;
				QList<IPresenceItem> pitems = presence!=NULL ? presence->presenceItems(ARosterItem.itemJid) : QList<IPresenceItem>();
				do
				{
					IRosterIndex *index;
					IPresenceItem pitem = pitems.value(presIndex++);
					if (pitem.isValid)
					{
						index = createRosterIndex(itemType,pitem.itemJid.pFull(),groupIndex);
						index->setData(RDR_JID,pitem.itemJid.full());
						index->setData(RDR_PJID,pitem.itemJid.pFull());
						index->setData(RDR_SHOW,pitem.show);
						index->setData(RDR_STATUS,pitem.status);
						index->setData(RDR_PRIORITY,pitem.priority);
					}
					else
					{
						index = createRosterIndex(itemType,ARosterItem.itemJid.pBare(),groupIndex);
						index->setData(RDR_JID,ARosterItem.itemJid.bare());
						index->setData(RDR_PJID,ARosterItem.itemJid.pBare());
					}

					index->setData(RDR_BARE_JID,ARosterItem.itemJid.pBare());
					index->setData(RDR_NAME,ARosterItem.name);
					index->setData(RDR_SUBSCRIBTION,ARosterItem.subscription);
					index->setData(RDR_ASK,ARosterItem.ask);
					index->setData(RDR_GROUP,group);

					itemList.append(index);
					insertRosterIndex(index,groupIndex);
				}
				while (presIndex < pitems.count());
			}
			else foreach(IRosterIndex *index,groupItemList)
			{
				index->setData(RDR_NAME,ARosterItem.name);
				index->setData(RDR_SUBSCRIBTION,ARosterItem.subscription);
				index->setData(RDR_ASK,ARosterItem.ask);
				itemList.append(index);
			}
		}

		foreach(IRosterIndex *index,curItemList)
		if (!itemList.contains(index))
			removeRosterIndex(index);
	}
}

void RostersModel::onRosterItemRemoved(IRoster *ARoster, const IRosterItem &ARosterItem)
{
	IRosterIndex *streamIndex = FStreamsRoot.value(ARoster->streamJid());
	if (streamIndex)
	{
		QMultiHash<int,QVariant> data;
		data.insert(RDR_TYPE,RIT_CONTACT);
		data.insert(RDR_TYPE,RIT_AGENT);
		data.insert(RDR_BARE_JID,ARosterItem.itemJid.pBare());
		QList<IRosterIndex *> itemList = streamIndex->findChild(data,true);
		foreach(IRosterIndex *index, itemList)
		removeRosterIndex(index);
	}
}

void RostersModel::onRosterStreamJidChanged(IRoster *ARoster, const Jid &ABefore)
{
	IRosterIndex *streamIndex = FStreamsRoot.value(ABefore);
	if (streamIndex)
	{
		Jid after = ARoster->streamJid();

		QHash<int,QVariant> data;
		data.insert(RDR_STREAM_JID,ABefore.pFull());
		QList<IRosterIndex *> itemList = FRootIndex->findChild(data,true);
		foreach(IRosterIndex *index, itemList)
		index->setData(RDR_STREAM_JID,after.pFull());

		streamIndex->setData(RDR_INDEX_ID,after.pFull());
		streamIndex->setData(RDR_JID,after.full());
		streamIndex->setData(RDR_PJID,after.pFull());

		FStreamsRoot.remove(ABefore);
		FStreamsRoot.insert(after,streamIndex);

		emit streamJidChanged(ABefore,after);
	}
}

void RostersModel::onPresenceChanged(IPresence *APresence, int AShow, const QString &AStatus, int APriority)
{
	IRosterIndex *streamIndex = FStreamsRoot.value(APresence->streamJid());
	if (streamIndex)
	{
		streamIndex->setData(RDR_SHOW, AShow);
		streamIndex->setData(RDR_STATUS, AStatus);
		if (AShow != IPresence::Offline && AShow != IPresence::Error)
			streamIndex->setData(RDR_PRIORITY, APriority);
		else
			streamIndex->setData(RDR_PRIORITY, QVariant());
	}
}

void RostersModel::onPresenceReceived(IPresence *APresence, const IPresenceItem &APresenceItem)
{
	IRosterIndex *streamIndex = FStreamsRoot.value(APresence->streamJid());
	if (streamIndex)
	{
		int itemType = RIT_CONTACT;
		if (APresenceItem.itemJid.node().isEmpty())
			itemType = RIT_AGENT;
		else if (APresenceItem.itemJid && APresence->streamJid())
			itemType = RIT_MY_RESOURCE;

		if (APresenceItem.show == IPresence::Offline)
		{
			QHash<int,QVariant> findData;
			findData.insert(RDR_TYPE,itemType);
			findData.insert(RDR_PJID,APresenceItem.itemJid.pFull());
			QList<IRosterIndex *> itemList = streamIndex->findChild(findData,true);
			int pitemsCount = APresence->presenceItems(APresenceItem.itemJid).count();
			foreach (IRosterIndex *index,itemList)
			{
				if (itemType == RIT_MY_RESOURCE || pitemsCount > 1)
				{
					removeRosterIndex(index);
				}
				else
				{
					index->setData(RDR_INDEX_ID,APresenceItem.itemJid.pBare());
					index->setData(RDR_JID,APresenceItem.itemJid.bare());
					index->setData(RDR_PJID,APresenceItem.itemJid.pBare());
					index->setData(RDR_SHOW,APresenceItem.show);
					index->setData(RDR_STATUS,APresenceItem.status);
					index->setData(RDR_PRIORITY,QVariant());
				}
			}
		}
		else if (APresenceItem.show == IPresence::Error)
		{
			QHash<int,QVariant> findData;
			findData.insert(RDR_TYPE,itemType);
			findData.insert(RDR_BARE_JID,APresenceItem.itemJid.pBare());
			QList<IRosterIndex *> itemList = streamIndex->findChild(findData,true);
			foreach(IRosterIndex *index,itemList)
			{
				index->setData(RDR_SHOW,APresenceItem.show);
				index->setData(RDR_STATUS,APresenceItem.status);
				index->setData(RDR_PRIORITY,QVariant());
			}
		}
		else
		{
			QHash<int,QVariant> findData;
			findData.insert(RDR_TYPE,itemType);
			findData.insert(RDR_PJID,APresenceItem.itemJid.pFull());
			QList<IRosterIndex *> itemList = streamIndex->findChild(findData,true);
			if (itemList.isEmpty())
			{
				IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(APresence->streamJid()) : NULL;
				IRosterItem ritem = roster!=NULL ? roster->rosterItem(APresenceItem.itemJid) : IRosterItem();
				QString groupDelim = roster!=NULL ? roster->groupDelimiter() : "::";

				QSet<QString> itemGroups;
				if (ritem.isValid)
					itemGroups = ritem.groups;
				if (itemGroups.isEmpty())
					itemGroups.insert("");

				foreach(QString group,itemGroups)
				{
					IRosterIndex *groupIndex = NULL;
					if (itemType == RIT_MY_RESOURCE)
						groupIndex = createGroup(myResourcesGroupName(),groupDelim,RIT_GROUP_MY_RESOURCES,streamIndex);
					else if (!ritem.isValid)
						groupIndex = createGroup(notInRosterGroupName(),groupDelim,RIT_GROUP_NOT_IN_ROSTER,streamIndex);
					else if (itemType == RIT_AGENT)
						groupIndex = findGroup(agentsGroupName(),groupDelim,RIT_GROUP_AGENTS,streamIndex);
					else if (group.isEmpty())
						groupIndex = findGroup(blankGroupName(),groupDelim,RIT_GROUP_BLANK,streamIndex);
					else
						groupIndex = findGroup(group,groupDelim,RIT_GROUP,streamIndex);

					if (groupIndex)
					{
						IRosterIndex *index = findRosterIndex(itemType,APresenceItem.itemJid.pBare(),groupIndex);
						if (!index)
						{
							index = createRosterIndex(itemType,APresenceItem.itemJid.pFull(),groupIndex);
							index->setData(RDR_BARE_JID,APresenceItem.itemJid.pBare());
							index->setData(RDR_GROUP,group);
							if (ritem.isValid)
							{
								index->setData(RDR_NAME,ritem.name);
								index->setData(RDR_SUBSCRIBTION,ritem.subscription);
								index->setData(RDR_ASK,ritem.ask);
							}
						}
						else
							index->setData(RDR_INDEX_ID,APresenceItem.itemJid.pFull());

						index->setData(RDR_JID,APresenceItem.itemJid.full());
						index->setData(RDR_PJID,APresenceItem.itemJid.pFull());
						index->setData(RDR_SHOW,APresenceItem.show);
						index->setData(RDR_STATUS,APresenceItem.status);
						index->setData(RDR_PRIORITY,APresenceItem.priority);
						insertRosterIndex(index,groupIndex);
					}
				}
			}
			else foreach(IRosterIndex *index,itemList)
			{
				index->setData(RDR_SHOW,APresenceItem.show);
				index->setData(RDR_STATUS,APresenceItem.status);
				index->setData(RDR_PRIORITY,APresenceItem.priority);
			}
		}
	}
}

void RostersModel::onIndexDataChanged(IRosterIndex *AIndex, int ARole)
{
	if (FChangedIndexes.isEmpty())
		QTimer::singleShot(0,this,SLOT(onDelayedDataChanged()));
	FChangedIndexes+=AIndex;
	emit indexDataChanged(AIndex, ARole);
}

void RostersModel::onIndexChildAboutToBeInserted(IRosterIndex *AIndex)
{
	emit indexAboutToBeInserted(AIndex);
	beginInsertRows(modelIndexByRosterIndex(AIndex->parentIndex()),AIndex->parentIndex()->childCount(),AIndex->parentIndex()->childCount());
	connect(AIndex->instance(),SIGNAL(dataChanged(IRosterIndex *, int)),
	        SLOT(onIndexDataChanged(IRosterIndex *, int)));
	connect(AIndex->instance(),SIGNAL(childAboutToBeInserted(IRosterIndex *)),
	        SLOT(onIndexChildAboutToBeInserted(IRosterIndex *)));
	connect(AIndex->instance(),SIGNAL(childInserted(IRosterIndex *)),
	        SLOT(onIndexChildInserted(IRosterIndex *)));
	connect(AIndex->instance(),SIGNAL(childAboutToBeRemoved(IRosterIndex *)),
	        SLOT(onIndexChildAboutToBeRemoved(IRosterIndex *)));
	connect(AIndex->instance(),SIGNAL(childRemoved(IRosterIndex *)),
	        SLOT(onIndexChildRemoved(IRosterIndex *)));
}

void RostersModel::onIndexChildInserted(IRosterIndex *AIndex)
{
	endInsertRows();
	emit indexInserted(AIndex);
}

void RostersModel::onIndexChildAboutToBeRemoved(IRosterIndex *AIndex)
{
	FChangedIndexes-=AIndex;
	emit indexAboutToBeRemoved(AIndex);
	beginRemoveRows(modelIndexByRosterIndex(AIndex->parentIndex()),AIndex->row(),AIndex->row());
}

void RostersModel::onIndexChildRemoved(IRosterIndex *AIndex)
{
	disconnect(AIndex->instance(),SIGNAL(dataChanged(IRosterIndex *, int)),
	           this,SLOT(onIndexDataChanged(IRosterIndex *, int)));
	disconnect(AIndex->instance(),SIGNAL(childAboutToBeInserted(IRosterIndex *)),
	           this,SLOT(onIndexChildAboutToBeInserted(IRosterIndex *)));
	disconnect(AIndex->instance(),SIGNAL(childInserted(IRosterIndex *)),
	           this,SLOT(onIndexChildInserted(IRosterIndex *)));
	disconnect(AIndex->instance(),SIGNAL(childAboutToBeRemoved(IRosterIndex *)),
	           this,SLOT(onIndexChildAboutToBeRemoved(IRosterIndex *)));
	disconnect(AIndex->instance(),SIGNAL(childRemoved(IRosterIndex *)),
	           this,SLOT(onIndexChildRemoved(IRosterIndex *)));
	endRemoveRows();
	emit indexRemoved(AIndex);
}

void RostersModel::onIndexDestroyed(IRosterIndex *AIndex)
{
	emit indexDestroyed(AIndex);
}

void RostersModel::onDelayedDataChanged()
{
	//Замена сигналов изменения большого числа индексов на сброс модели
	//из-за тормозов в сортировке в QSortFilterProxyModel
	if (FChangedIndexes.count() < INDEX_CHANGES_FOR_RESET)
	{
		//Вызывает dataChanged у всех родителей для поддержки SortFilterProxyModel
		QSet<IRosterIndex *> childIndexes = FChangedIndexes;
		foreach(IRosterIndex *index,childIndexes)
		{
			IRosterIndex *parentIndex = index->parentIndex();
			while (parentIndex && !FChangedIndexes.contains(parentIndex))
			{
				FChangedIndexes+=parentIndex;
				parentIndex = parentIndex->parentIndex();
			}
			QModelIndex modelIndex = modelIndexByRosterIndex(index);
			emit dataChanged(modelIndex,modelIndex);
		}
		emitDelayedDataChanged(FRootIndex);
	}
	else
		reset();

	FChangedIndexes.clear();
}

Q_EXPORT_PLUGIN2(plg_rostersmodel, RostersModel)
