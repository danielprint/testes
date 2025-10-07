#include "MainWindow.h"
#include "GLViewport.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QStringList>
#include <QToolBar>
#include <QVBoxLayout>
#include <QObject>

namespace
{
constexpr int kMaxRecentFiles = 5;

QDoubleSpinBox *createSpinBox(double min, double max, double step)
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(3);
    spin->setSingleStep(step);
    spin->setAlignment(Qt::AlignRight);
    return spin;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    createUi();
    readSettings();
    populateRecentFiles();
    statusBar()->showMessage(tr("LMB orbit • MMB pan • Wheel zoom • F toggles fly mode"));
}

MainWindow::~MainWindow()
{
    writeSettings();
}

void MainWindow::createUi()
{
    setWindowTitle(tr("STL Viewer"));
    resize(1280, 800);

    m_viewport = new GLViewport(this);
    setCentralWidget(m_viewport);

    createDock();
    createMenus();

    m_statusCameraLabel = new QLabel(tr("Dist: --"));
    m_statusFpsLabel = new QLabel(tr("FPS: --"));
    statusBar()->addPermanentWidget(m_statusCameraLabel);
    statusBar()->addPermanentWidget(m_statusFpsLabel);

    connect(m_viewport, &GLViewport::meshInfoChanged, this, &MainWindow::updateMeshInfo);
    connect(m_viewport, &GLViewport::cameraDistanceChanged, this, &MainWindow::updateCameraStatus);
    connect(m_viewport, &GLViewport::fpsChanged, this, &MainWindow::updateFps);
    connect(m_viewport, &GLViewport::loadFailed, this, &MainWindow::handleLoadFailure);
    connect(m_viewport, &GLViewport::transformChanged, this, [this](const QVector3D &t, const QVector3D &r, const QVector3D &s) {
        m_ignoreTransformSignal = true;
        for (int i = 0; i < 3; ++i) {
            if (m_translate[i])
                m_translate[i]->setValue(t[i]);
            if (m_rotate[i])
                m_rotate[i]->setValue(r[i]);
            if (m_scale[i])
                m_scale[i]->setValue(s[i]);
        }
        m_ignoreTransformSignal = false;
    });
}

void MainWindow::createDock()
{
    m_sidebar = new QDockWidget(tr("Controls"), this);
    m_sidebar->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *openButton = new QPushButton(tr("Open STL…"));
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openFileDialog);
    layout->addWidget(openButton);

    m_recentList = new QListWidget;
    m_recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recentList->setMaximumHeight(110);
    layout->addWidget(m_recentList);
    connect(m_recentList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item)
            loadFile(item->data(Qt::UserRole).toString());
    });

    m_infoLabel = new QLabel(tr("No model loaded"));
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setTextFormat(Qt::RichText);
    layout->addWidget(m_infoLabel);

    auto *renderLabel = new QLabel(tr("Render Settings"));
    renderLabel->setStyleSheet("font-weight: bold");
    layout->addWidget(renderLabel);

    m_gridCheck = new QCheckBox(tr("Show Grid"));
    m_axisCheck = new QCheckBox(tr("Show Axes"));
    m_cullingCheck = new QCheckBox(tr("Backface Culling"));
    m_normalsCheck = new QCheckBox(tr("Recompute Vertex Normals"));
    m_faceNormalCheck = new QCheckBox(tr("Use Face Normals"));

    for (QCheckBox *box : {m_gridCheck, m_axisCheck, m_cullingCheck, m_normalsCheck, m_faceNormalCheck}) {
        layout->addWidget(box);
        connect(box, &QCheckBox::toggled, this, &MainWindow::applyRenderToggles);
    }

    m_shadingCombo = new QComboBox;
    m_shadingCombo->addItems({tr("Shaded"), tr("Wireframe"), tr("Shaded + Wireframe")});
    layout->addWidget(m_shadingCombo);
    connect(m_shadingCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::toggleShadingMode);

    auto *transformLabel = new QLabel(tr("Transform"));
    transformLabel->setStyleSheet("font-weight: bold");
    layout->addWidget(transformLabel);

    auto *transformWidget = new QWidget;
    auto *form = new QFormLayout(transformWidget);
    form->setLabelAlignment(Qt::AlignLeft);

    const QString axes[3] = {tr("X"), tr("Y"), tr("Z")};
    for (int i = 0; i < 3; ++i) {
        m_translate[i] = createSpinBox(-1000.0, 1000.0, 1.0);
        form->addRow(tr("T%1 (mm)").arg(axes[i]), m_translate[i]);
        connect(m_translate[i], qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateTransformFromUi);
    }
    for (int i = 0; i < 3; ++i) {
        m_rotate[i] = createSpinBox(-720.0, 720.0, 1.0);
        form->addRow(tr("R%1 (°)").arg(axes[i]), m_rotate[i]);
        connect(m_rotate[i], qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateTransformFromUi);
    }
    for (int i = 0; i < 3; ++i) {
        m_scale[i] = createSpinBox(0.001, 1000.0, 0.01);
        m_scale[i]->setValue(1.0);
        form->addRow(tr("S%1").arg(axes[i]), m_scale[i]);
        connect(m_scale[i], qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateTransformFromUi);
    }

    layout->addWidget(transformWidget);

    auto *resetButton = new QPushButton(tr("Reset Transform"));
    layout->addWidget(resetButton);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetTransform);

    layout->addStretch(1);

    m_sidebar->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebar);
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    m_openAction = fileMenu->addAction(tr("&Open…"), this, &MainWindow::openFileDialog, QKeySequence::Open);
    m_recentMenu = fileMenu->addMenu(tr("Recent Files"));
    for (int i = 0; i < kMaxRecentFiles; ++i) {
        auto *action = new QAction(this);
        action->setVisible(false);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        m_recentMenu->addAction(action);
        m_recentFileActions.append(action);
    }
    fileMenu->addSeparator();

    m_screenshotAction = fileMenu->addAction(tr("Save Screenshot"), this, &MainWindow::saveScreenshot);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);
}

void MainWindow::openFileDialog()
{
    QSettings settings;
    const QString dir = settings.value("lastDirectory", QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open STL"), dir, tr("STL Files (*.stl)"));
    if (path.isEmpty())
        return;

    settings.setValue("lastDirectory", QFileInfo(path).absolutePath());
    loadFile(path);
}

void MainWindow::openRecentFile()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    const QString path = action->data().toString();
    loadFile(path);
}

void MainWindow::loadFile(const QString &path)
{
    if (path.isEmpty())
        return;

    QProgressDialog progress(tr("Loading %1").arg(QFileInfo(path).fileName()), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setAutoClose(true);
    progress.setCancelButton(nullptr);
    progress.show();
    QApplication::processEvents();

    QString errorMessage;
    if (!m_viewport->loadMesh(path, &errorMessage)) {
        progress.close();
        if (!errorMessage.isEmpty())
            handleLoadFailure(errorMessage);
        return;
    }

    progress.close();
    setWindowFilePath(path);
    m_currentFilePath = path;
    addRecentFile(path);
    refreshModelDetails();
}

void MainWindow::updateMeshInfo(const MeshStatistics &stats)
{
    m_currentStats = stats;
    refreshModelDetails();
}

void MainWindow::refreshModelDetails()
{
    if (m_currentStats.fileName.isEmpty()) {
        m_infoLabel->setText(tr("No model loaded"));
        return;
    }

    const QVector3D min = m_currentStats.minBounds;
    const QVector3D max = m_currentStats.maxBounds;
    const QVector3D size = m_currentStats.size;

    const QString info = tr(
        "<b>%1</b><br/>Triangles: %2<br/>Bounds min: (%3, %4, %5) mm<br/>Bounds max: (%6, %7, %8) mm<br/>Size: (%9, %10, %11) mm<br/>Normals: %12")
                              .arg(m_currentStats.fileName.toHtmlEscaped())
                              .arg(QString::number(m_currentStats.triangleCount))
                              .arg(QString::number(min.x(), 'f', 2))
                              .arg(QString::number(min.y(), 'f', 2))
                              .arg(QString::number(min.z(), 'f', 2))
                              .arg(QString::number(max.x(), 'f', 2))
                              .arg(QString::number(max.y(), 'f', 2))
                              .arg(QString::number(max.z(), 'f', 2))
                              .arg(QString::number(size.x(), 'f', 2))
                              .arg(QString::number(size.y(), 'f', 2))
                              .arg(QString::number(size.z(), 'f', 2))
                              .arg(m_currentStats.hasNormals ? tr("Provided") : tr("Generated"));

    m_infoLabel->setText(info);
}

void MainWindow::updateCameraStatus(float distance)
{
    if (!m_statusCameraLabel)
        return;
    m_statusCameraLabel->setText(tr("Dist: %1 mm").arg(distance, 0, 'f', 2));
}

void MainWindow::updateFps(float fps)
{
    if (!m_statusFpsLabel)
        return;
    m_statusFpsLabel->setText(tr("FPS: %1").arg(fps, 0, 'f', 1));
}

void MainWindow::handleLoadFailure(const QString &message)
{
    QMessageBox::critical(this, tr("Failed to load STL"), message);
}

void MainWindow::saveScreenshot()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), QString(), tr("PNG Image (*.png)"));
    if (path.isEmpty())
        return;
    if (!m_viewport->saveScreenshot(path)) {
        QMessageBox::warning(this, tr("Screenshot"), tr("Failed to save screenshot."));
    }
}

void MainWindow::updateTransformFromUi()
{
    if (m_ignoreTransformSignal)
        return;

    QVector3D t, r, s;
    for (int i = 0; i < 3; ++i) {
        t[i] = m_translate[i] ? static_cast<float>(m_translate[i]->value()) : 0.0f;
        r[i] = m_rotate[i] ? static_cast<float>(m_rotate[i]->value()) : 0.0f;
        s[i] = m_scale[i] ? static_cast<float>(m_scale[i]->value()) : 1.0f;
    }
    m_viewport->setModelTransform(t, r, s);
}

void MainWindow::resetTransform()
{
    m_viewport->resetModelTransform();
}

void MainWindow::toggleShadingMode(int index)
{
    m_viewport->setShadingMode(static_cast<GLViewport::ShadingMode>(index));
}

void MainWindow::applyRenderToggles()
{
    if (!m_viewport)
        return;
    m_viewport->setGridVisible(m_gridCheck->isChecked());
    m_viewport->setAxesVisible(m_axisCheck->isChecked());
    m_viewport->setBackfaceCullingEnabled(m_cullingCheck->isChecked());
    m_viewport->setRecomputeNormals(m_normalsCheck->isChecked());
    m_viewport->setFaceNormalsEnabled(m_faceNormalCheck->isChecked());
}

void MainWindow::populateRecentFiles()
{
    const QStringList files = recentFiles();
    for (int i = 0; i < m_recentFileActions.size(); ++i) {
        if (i < files.size()) {
            const QString text = QFileInfo(files.at(i)).fileName();
            m_recentFileActions[i]->setText(tr("%1 %2").arg(i + 1).arg(text));
            m_recentFileActions[i]->setData(files.at(i));
            m_recentFileActions[i]->setVisible(true);
        } else {
            m_recentFileActions[i]->setVisible(false);
        }
    }

    if (m_recentList) {
        m_recentList->clear();
        for (const QString &file : files) {
            auto *item = new QListWidgetItem(QFileInfo(file).fileName());
            item->setToolTip(file);
            item->setData(Qt::UserRole, file);
            m_recentList->addItem(item);
        }
    }
}

void MainWindow::addRecentFile(const QString &path)
{
    QStringList files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles)
        files.removeLast();
    setRecentFiles(files);
    populateRecentFiles();
}

QStringList MainWindow::recentFiles() const
{
    QSettings settings;
    return settings.value("recentFiles").toStringList();
}

void MainWindow::setRecentFiles(const QStringList &paths)
{
    QSettings settings;
    settings.setValue("recentFiles", paths);
}

void MainWindow::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    m_gridCheck->setChecked(settings.value("render/showGrid", true).toBool());
    m_axisCheck->setChecked(settings.value("render/showAxes", true).toBool());
    m_cullingCheck->setChecked(settings.value("render/backfaceCulling", false).toBool());
    m_normalsCheck->setChecked(settings.value("render/recomputeNormals", false).toBool());
    m_faceNormalCheck->setChecked(settings.value("render/faceNormals", false).toBool());
    m_shadingCombo->setCurrentIndex(settings.value("render/shadingMode", 0).toInt());
    applyRenderToggles();
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("render/showGrid", m_gridCheck->isChecked());
    settings.setValue("render/showAxes", m_axisCheck->isChecked());
    settings.setValue("render/backfaceCulling", m_cullingCheck->isChecked());
    settings.setValue("render/recomputeNormals", m_normalsCheck->isChecked());
    settings.setValue("render/faceNormals", m_faceNormalCheck->isChecked());
    settings.setValue("render/shadingMode", m_shadingCombo->currentIndex());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    QMainWindow::closeEvent(event);
}
