#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector3D>

#include "MeshStatistics.h"

class QAction;
class QLabel;
class QListWidget;
class QDoubleSpinBox;
class QDockWidget;
class QMenu;
class QPushButton;
class QCheckBox;
class QComboBox;
class QSettings;
class QListWidget;

class GLViewport;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openFileDialog();
    void openRecentFile();
    void updateMeshInfo(const MeshStatistics &stats);
    void updateCameraStatus(float distance);
    void updateFps(float fps);
    void handleLoadFailure(const QString &message);
    void saveScreenshot();
    void updateTransformFromUi();
    void resetTransform();
    void toggleShadingMode(int index);
    void applyRenderToggles();

private:
    void createUi();
    void createDock();
    void createMenus();
    void populateRecentFiles();
    void addRecentFile(const QString &path);
    QStringList recentFiles() const;
    void setRecentFiles(const QStringList &paths);
    void loadFile(const QString &path);
    void readSettings();
    void writeSettings();
    void refreshModelDetails();

    GLViewport *m_viewport = nullptr;
    QDockWidget *m_sidebar = nullptr;

    QAction *m_openAction = nullptr;
    QAction *m_screenshotAction = nullptr;
    QMenu *m_recentMenu = nullptr;
    QList<QAction *> m_recentFileActions;
    QListWidget *m_recentList = nullptr;

    QLabel *m_infoLabel = nullptr;
    QLabel *m_statusCameraLabel = nullptr;
    QLabel *m_statusFpsLabel = nullptr;

    QCheckBox *m_gridCheck = nullptr;
    QCheckBox *m_axisCheck = nullptr;
    QCheckBox *m_cullingCheck = nullptr;
    QCheckBox *m_normalsCheck = nullptr;
    QCheckBox *m_faceNormalCheck = nullptr;

    QComboBox *m_shadingCombo = nullptr;

    QDoubleSpinBox *m_translate[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox *m_rotate[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox *m_scale[3] = {nullptr, nullptr, nullptr};

    MeshStatistics m_currentStats;
    QString m_currentFilePath;

    bool m_ignoreTransformSignal = false;
};
