#pragma once

#include "model/GraphDocument.h"

#include <QMainWindow>
#include <QHash>
#include <QMessageBox>
#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>

class EditorScene;
class GraphView;
class QDockWidget;
class QMenu;
class QAction;
class QTableWidget;
class QTabWidget;
class QUndoStack;
class QUndoGroup;
class QCloseEvent;
class NodeItem;
class ProjectTreePanel;
class PropertyPanel;
class PalettePanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Automation/test helpers
    int documentCount() const;
    int activeDocumentIndex() const;
    bool isDocumentDirty(int index) const;
    QString documentFilePath(int index) const;
    EditorScene* activeScene() const;

    int newDocument(const QString& title = QString());
    bool openDocumentByDialog();
    bool openDocumentFromPath(const QString& path);
    bool saveCurrentDocument(bool saveAs = false);
    bool closeDocument(int index = -1);

    void setOpenFileDialogProvider(const std::function<QString()>& provider);
    void setSaveFileDialogProvider(const std::function<QString(const QString& suggested)>& provider);
    void setUnsavedPromptProvider(const std::function<QMessageBox::StandardButton(const QString& docTitle)>& provider);
    void setCriticalMessageProvider(const std::function<void(const QString& title, const QString& text)>& provider);
    void clearDialogProviders();

private:
    struct DocumentContext {
        EditorScene* scene = nullptr;
        GraphView* view = nullptr;
        QUndoStack* undoStack = nullptr;
        QString title;
        QString filePath;
        bool dirty = false;
        bool suppressDirtyTracking = false;
    };

    void setupWindow();
    void setupMenusAndToolbar();
    void setupCentralArea();
    void setupLeftDocks();
    void setupRightDock();
    void setupSignalBindings();
    void populateDemoGraph();
    int createEditorTab(const QString& title, const GraphDocument* initialDocument = nullptr, const QString& filePath = QString());
    void activateEditorTab(int index);
    int documentIndexForScene(const EditorScene* scene) const;
    int documentIndexForUndoStack(const QUndoStack* stack) const;
    int currentDocumentIndex() const;
    QString requestOpenFilePath() const;
    QString requestSaveFilePath(const QString& suggested) const;
    QMessageBox::StandardButton requestUnsavedDecision(const QString& docTitle) const;
    void showCriticalMessage(const QString& title, const QString& text) const;
    bool saveDocument(int index, bool saveAs);
    bool maybeSaveDocument(int index);
    bool closeDocumentTab(int index);
    void setDocumentDirty(int index, bool dirty);
    void updateTabTitle(int index);
    void updatePropertyTable(const QString& itemType,
                             const QString& itemId,
                             const QString& displayName,
                             const QPointF& pos,
                             int inputCount,
                             int outputCount);
    void rebuildProjectTreeNodes();
    void onPropertyCellChanged(int row, int column);
    NodeItem* findNodeById(const QString& nodeId) const;
    void closeEvent(QCloseEvent* event) override;

    QMenu* m_viewMenu = nullptr;
    QTabWidget* m_editorTabs = nullptr;
    GraphView* m_graphView = nullptr;
    EditorScene* m_scene = nullptr;
    QTableWidget* m_propertyTable = nullptr;

    QDockWidget* m_projectDock = nullptr;
    QDockWidget* m_propertyDock = nullptr;
    QDockWidget* m_paletteDock = nullptr;

    ProjectTreePanel* m_projectPanel = nullptr;
    PropertyPanel* m_propertyPanel = nullptr;
    PalettePanel* m_palettePanel = nullptr;
    QUndoGroup* m_undoGroup = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QString m_selectedItemType;
    QString m_selectedItemId;
    QHash<int, PropertyData> m_dynamicPropertyRows;
    bool m_propertyTableUpdating = false;
    int m_untitledCounter = 1;
    QVector<DocumentContext> m_documents;

    std::function<QString()> m_openFileDialogProvider;
    std::function<QString(const QString& suggested)> m_saveFileDialogProvider;
    std::function<QMessageBox::StandardButton(const QString& docTitle)> m_unsavedPromptProvider;
    std::function<void(const QString& title, const QString& text)> m_criticalMessageProvider;
};
