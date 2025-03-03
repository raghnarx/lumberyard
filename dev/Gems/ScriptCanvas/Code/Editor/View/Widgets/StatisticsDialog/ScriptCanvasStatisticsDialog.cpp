/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include <precompiled.h>

#include <Editor/View/Widgets/StatisticsDialog/ScriptCanvasStatisticsDialog.h>
#include <Editor/View/Widgets/StatisticsDialog/ui_ScriptCanvasStatisticsDialog.h>

#include <Editor/GraphCanvas/GraphCanvasEditorNotificationBusId.h>

#include <ScriptCanvas/Assets/ScriptCanvasAsset.h>
#include <ScriptCanvas/Bus/RequestBus.h>

namespace
{
    ScriptCanvasEditor::NodePaletteNodeUsageRootItem* ExternalCreatePaletteRoot(const ScriptCanvasEditor::NodePaletteModel& nodePaletteModel)
    {
        ScriptCanvasEditor::NodePaletteNodeUsageRootItem* root = aznew ScriptCanvasEditor::NodePaletteNodeUsageRootItem(nodePaletteModel);

        const ScriptCanvasEditor::NodePaletteModel::NodePaletteRegistry& nodeRegistry = nodePaletteModel.GetNodeRegistry();

        for (const auto& registryPair : nodeRegistry)
        {
            const ScriptCanvasEditor::NodePaletteModelInformation* modelInformation = registryPair.second;

            GraphCanvas::GraphCanvasTreeItem* parentItem = root->GetCategoryNode(modelInformation->m_categoryPath.c_str());
            GraphCanvas::NodePaletteTreeItem* createdItem = nullptr;

            createdItem = parentItem->CreateChildNode<ScriptCanvasEditor::NodePaletteNodeUsagePaletteItem>(modelInformation->m_nodeIdentifier, modelInformation->m_displayName);

            if (createdItem)
            {
                modelInformation->PopulateTreeItem((*createdItem));
            }
        }

        root->PruneEmptyNodes();

        return root;
    }
}

namespace ScriptCanvasEditor
{
    //////////////////////////////////////////
    // ScriptCanvasAssetNodeUsageFilterModel
    //////////////////////////////////////////

    ScriptCanvasAssetNodeUsageFilterModel::ScriptCanvasAssetNodeUsageFilterModel()
        : m_nodeIdentifier(0)
    {

    }

    bool ScriptCanvasAssetNodeUsageFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
    {
        // Never want to show something if we don't have a node type identifier
        if (m_nodeIdentifier == 0)
        {
            return false;
        }

        QAbstractItemModel* model = sourceModel();
        QModelIndex index = model->index(sourceRow, 0, sourceParent);

        ScriptCanvasAssetNodeUsageTreeItem* treeItem = static_cast<ScriptCanvasAssetNodeUsageTreeItem*>(index.internalPointer());

        if (!treeItem->GetAssetId().IsValid())
        {
            for (int i = 0; i < treeItem->GetChildCount(); ++i)
            {
                if (filterAcceptsRow(i, index))
                {
                    return true;
                }
            }
        }
        else
        {
            treeItem->SetActiveNodeType(m_nodeIdentifier);

            if (treeItem->GetNodeCount() == 0)
            {
                return false;
            }

            bool showRow = m_filter.isEmpty();

            if (!showRow)
            {
                const QString& name = treeItem->GetName();

                int regexIndex = name.lastIndexOf(m_regex);

                showRow = regexIndex >= 0;

                if (!showRow)
                {
                    ScriptCanvasAssetNodeUsageTreeItem* parentItem = static_cast<ScriptCanvasAssetNodeUsageTreeItem*>(treeItem->GetParent());

                    while (!showRow && parentItem)
                    {
                        ScriptCanvasAssetNodeUsageTreeItem* nextItem = static_cast<ScriptCanvasAssetNodeUsageTreeItem*>(parentItem->GetParent());

                        // This means we are the root element. And we don't want to match based on it.
                        if (nextItem == nullptr)
                        {
                            break;
                        }

                        const QString& parentName = parentItem->GetName();

                        int regexIndex = parentName.lastIndexOf(m_regex);

                        if (regexIndex >= 0)
                        {
                            showRow = true;
                        }

                        parentItem = nextItem;
                    }
                }
            }

            return showRow;
        }

        return false;
    }

    void ScriptCanvasAssetNodeUsageFilterModel::SetFilter(const QString& filterName)
    {
        m_filter = filterName;
        m_regex = QRegExp(m_filter, Qt::CaseInsensitive);

        invalidate();
    }

    void ScriptCanvasAssetNodeUsageFilterModel::SetNodeTypeFilter(const ScriptCanvas::NodeTypeIdentifier& nodeType)
    {
        m_nodeIdentifier = nodeType;        

        invalidate();
    }

    /////////////////////
    // StatisticsDialog
    /////////////////////

    StatisticsDialog::StatisticsDialog(const NodePaletteModel& nodePaletteModel, AzToolsFramework::AssetBrowser::AssetBrowserFilterModel* scriptCanvasAssetBrowserModel, QWidget* widget)
        : QDialog(widget)
        , m_nodePaletteModel(nodePaletteModel)
        , m_ui(new Ui::ScriptCanvasStatisticsDialog())
        , m_treeRoot(nullptr)
        , m_scriptCanvasAssetBrowserModel(scriptCanvasAssetBrowserModel)
        , m_scriptCanvasAssetTreeRoot(nullptr)
        , m_scriptCanvasAssetTree(nullptr)
        , m_scriptCanvasAssetFilterModel(nullptr)
    {
        setWindowFlags(Qt::WindowFlags::enum_type::WindowCloseButtonHint);

        m_ui->setupUi(this);
    }

    StatisticsDialog::~StatisticsDialog()
    {
    }

    void StatisticsDialog::AssetChanged(AzFramework::AssetSystem::AssetNotificationMessage message)
    {
        // Might need to limit this based upon the message. But no clear message type jumps out at me.
        if (message.m_assetType == azrtti_typeid<ScriptCanvasAsset>())
        {
            m_scriptCanvasAssetTreeRoot->RegisterAsset(message.m_assetId);
        }
    }

    void StatisticsDialog::AssetRemoved(AzFramework::AssetSystem::AssetNotificationMessage message)
    {
        if (message.m_assetType == azrtti_typeid<ScriptCanvasAsset>())
        {
            m_scriptCanvasAssetTreeRoot->RegisterAsset(message.m_assetId);
        }
    }

    void StatisticsDialog::OnScriptCanvasAssetClicked(const QModelIndex& index)
    {
        if (index.isValid())
        {
            if (index.column() == ScriptCanvasAssetNodeUsageTreeItem::Column::OpenIcon)
            {
                QModelIndex sourceIndex = m_scriptCanvasAssetFilterModel->mapToSource(index);
                ScriptCanvasAssetNodeUsageTreeItem* treeItem = static_cast<ScriptCanvasAssetNodeUsageTreeItem*>(sourceIndex.internalPointer());

                if (treeItem->GetAssetId().IsValid())
                {
                    GeneralRequestBus::Broadcast(&GeneralRequests::OpenScriptCanvasAssetId, treeItem->GetAssetId());
                }
            }
        }
    }

    void StatisticsDialog::showEvent(QShowEvent* showEvent)
    {
        InitStatisticsWindow();

        QDialog::showEvent(showEvent);
    }

    void StatisticsDialog::OnSelectionCleared()
    {
        m_scriptCanvasAssetFilterModel->SetNodeTypeFilter(0);
        
        m_ui->statDisplayName->setText("N/A");

        m_ui->totalUsageCount->setText(QString::number(0));
        m_ui->uniqueGraphsCount->setText(QString::number(0));
        m_ui->averageGraphUsages->setText(QString::number(0));
    }

    void StatisticsDialog::OnItemSelected(const GraphCanvas::GraphCanvasTreeItem* treeItem)
    {
        const NodePaletteNodeUsagePaletteItem* usageItem = azrtti_cast<const NodePaletteNodeUsagePaletteItem*>(treeItem);

        if (usageItem)
        {
            m_scriptCanvasAssetFilterModel->SetNodeTypeFilter(usageItem->GetNodeTypeIdentifier());
            m_ui->scriptCanvasAssetTree->expandAll();

            const ScriptCanvasAssetNodeUsageTreeItemRoot::ScriptCanvasAssetMap& assetMapping = m_scriptCanvasAssetTreeRoot->GetAssetTreeItems();

            int totalNodeCount = 0;
            int uniqueGraphs = 0;            

            for (auto itemPair : assetMapping)
            {
                int nodeCount = itemPair.second->GetNodeCount();

                if (nodeCount > 0)
                {
                    totalNodeCount += nodeCount;
                    uniqueGraphs++;
                }
            }

            m_ui->statDisplayName->setText(usageItem->GetName());

            m_ui->totalUsageCount->setText(QString::number(totalNodeCount));
            m_ui->uniqueGraphsCount->setText(QString::number(uniqueGraphs));

            float averageUses = 0;

            if (uniqueGraphs != 0)
            {
                averageUses = static_cast<float>(totalNodeCount) / static_cast<float>(uniqueGraphs);
            }

            m_ui->averageGraphUsages->setText(QString::number(averageUses, 'g', 2));
        }
        else
        {
            OnSelectionCleared();
        }
    }

    void StatisticsDialog::OnFilterUpdated(const QString& filterText)
    {
        m_scriptCanvasAssetFilterModel->SetFilter(filterText);

        m_ui->scriptCanvasAssetTree->expandAll();
    }

    void StatisticsDialog::OnScriptCanvasAssetRowsInserted(QModelIndex parentIndex, int first, int last)
    {
        for (int i = first; i <= last; ++i)
        {
            QModelIndex modelIndex = m_scriptCanvasAssetBrowserModel->index(first, 0, parentIndex);
            QModelIndex sourceIndex = m_scriptCanvasAssetBrowserModel->mapToSource(modelIndex);

            const AzToolsFramework::AssetBrowser::AssetBrowserEntry* entry = reinterpret_cast<AzToolsFramework::AssetBrowser::AssetBrowserEntry*>(sourceIndex.internalPointer());

            ProcessAsset(entry);
        }
    }

    void StatisticsDialog::InitStatisticsWindow()
    {
        if (m_treeRoot == nullptr)
        {
            m_treeRoot = ExternalCreatePaletteRoot(m_nodePaletteModel);

            GraphCanvas::NodePaletteConfig paletteConfig;

            paletteConfig.m_rootTreeItem = m_treeRoot;
            paletteConfig.m_editorId = ScriptCanvasEditor::AssetEditorId;

            paletteConfig.m_mimeType = "";

            paletteConfig.m_isInContextMenu = false;
            paletteConfig.m_saveIdentifier = "ScriptCanvas_UsageStatistics";
            paletteConfig.m_clearSelectionOnSceneChange = false;

            m_ui->nodePaletteWidget->SetupNodePalette(paletteConfig);

            m_scriptCanvasAssetTreeRoot = aznew ScriptCanvasAssetNodeUsageTreeItemRoot();
            m_scriptCanvasAssetTree = aznew GraphCanvas::GraphCanvasTreeModel(m_scriptCanvasAssetTreeRoot);

            m_scriptCanvasAssetFilterModel = aznew ScriptCanvasAssetNodeUsageFilterModel();
            m_scriptCanvasAssetFilterModel->setSourceModel(m_scriptCanvasAssetTree);

            TraverseTree();

            AzFramework::AssetSystemBus::Handler::BusConnect();

            m_ui->scriptCanvasAssetTree->setModel(m_scriptCanvasAssetFilterModel);

            m_ui->splitter->setStretchFactor(0, 1);
            m_ui->splitter->setStretchFactor(1, 2);

            m_ui->searchWidget->SetFilterInputInterval(AZStd::chrono::milliseconds(250));

            m_ui->scriptCanvasAssetTree->header()->setSectionResizeMode(ScriptCanvasAssetNodeUsageTreeItem::Column::Name, QHeaderView::ResizeMode::ResizeToContents);

            m_ui->scriptCanvasAssetTree->header()->setSectionResizeMode(ScriptCanvasAssetNodeUsageTreeItem::Column::UsageCount, QHeaderView::ResizeMode::Fixed);
            m_ui->scriptCanvasAssetTree->header()->resizeSection(ScriptCanvasAssetNodeUsageTreeItem::Column::UsageCount, 30);

            QObject::connect(m_ui->scriptCanvasAssetTree, &QTreeView::clicked, this, &StatisticsDialog::OnScriptCanvasAssetClicked);

            QObject::connect(m_ui->nodePaletteWidget, &GraphCanvas::NodePaletteWidget::OnSelectionCleared, this, &StatisticsDialog::OnSelectionCleared);
            QObject::connect(m_ui->nodePaletteWidget, &GraphCanvas::NodePaletteWidget::OnTreeItemSelected, this, &StatisticsDialog::OnItemSelected);

            QObject::connect(m_ui->searchWidget, &AzQtComponents::FilteredSearchWidget::TextFilterChanged, this, &StatisticsDialog::OnFilterUpdated);

            QObject::connect(m_scriptCanvasAssetBrowserModel, &QAbstractItemModel::rowsInserted, this, &StatisticsDialog::OnScriptCanvasAssetRowsInserted);

            OnSelectionCleared();
        }
    }

    void StatisticsDialog::ResetModel()
    {
        if (m_treeRoot)
        {
            m_treeRoot = ExternalCreatePaletteRoot(m_nodePaletteModel);
            m_ui->nodePaletteWidget->ResetModel(m_treeRoot);
        }
    }

    void StatisticsDialog::TraverseTree(QModelIndex index)
    {
        QModelIndex sourceIndex = m_scriptCanvasAssetBrowserModel->mapToSource(index);
        AzToolsFramework::AssetBrowser::AssetBrowserEntry* entry = reinterpret_cast<AzToolsFramework::AssetBrowser::AssetBrowserEntry*>(sourceIndex.internalPointer());

        ProcessAsset(entry);

        int rowCount = m_scriptCanvasAssetBrowserModel->rowCount(index);

        for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex nextIndex = m_scriptCanvasAssetBrowserModel->index(i, 0, index);
            TraverseTree(nextIndex);
        }
    }

    void StatisticsDialog::ProcessAsset(const AzToolsFramework::AssetBrowser::AssetBrowserEntry* entry)
    {
        if (entry)
        {
            if (entry->GetEntryType() == AzToolsFramework::AssetBrowser::AssetBrowserEntry::AssetEntryType::Product)
            {
                const AzToolsFramework::AssetBrowser::ProductAssetBrowserEntry* productEntry = static_cast<const AzToolsFramework::AssetBrowser::ProductAssetBrowserEntry*>(entry);

                if (productEntry->GetAssetType() == azrtti_typeid<ScriptCanvasAsset>())
                {
                    const AZ::Data::AssetId& assetId = productEntry->GetAssetId();

                    m_scriptCanvasAssetTreeRoot->RegisterAsset(assetId);
                }
            }
        }
    }

#include <Editor/View/Widgets/StatisticsDialog/ScriptCanvasStatisticsDialog.moc>
}