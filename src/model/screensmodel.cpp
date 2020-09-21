#include "screensmodel.hpp"
#include "commands/attrcommand.hpp"
#include "repository/skinrepository.hpp"
#include "model/windowstyle.hpp"
#include "skin/includefile.hpp"
#include <QUndoStack>
#include <QMimeData>
#include <QByteArray>
#include <QDataStream>

// ScreensTree

void ScreensTree::loadPreviews(const QString& path)
{
    m_previews.clear();

    QFile file(path);
    bool ok = file.open(QIODevice::ReadOnly);
    if (!ok)
        return;

    QXmlStreamReader xml(&file);

    while (xml.readNextStartElement()) {
        if (xml.name() == "screens")
            break;
        xml.skipCurrentElement();
    }
    // we are inside screens
    while (xml.readNextStartElement()) {
        if (xml.name() != "screen") {
            xml.skipCurrentElement();
            continue;
        }

        // we are inside screen
        QString screenName;
        QMap<QString, Preview> map;

        while (xml.readNextStartElement()) {
            if (xml.name() == "name") {
                screenName = xml.readElementText();

            } else if (xml.name() == "entries") {
                for (; xml.readNextStartElement(); xml.skipCurrentElement()) {
                    if (xml.name() != "entry")
                        continue;

                    // we are inside entry
                    const QString widgetName = xml.attributes().value("name").toString();
                    const QString value = xml.attributes().value("value").toString();
                    QString str = xml.attributes().value("render").toString();
                    auto render = EnumAttr<Property::Render>(str).value();
                    if (!widgetName.isEmpty()) {
                        map.insert(widgetName, Preview(value, render));
                    }
                }
            } else {
                xml.skipCurrentElement();
            }
        }
        if (!screenName.isEmpty()) {
            m_previews.insert(screenName, map);
        }
    }
}

Preview ScreensTree::getPreview(const QString& screen, const QString& widget) const
{
    auto screen_it = m_previews.find(screen);
    if (screen_it == m_previews.end())
        return Preview();
    auto widget_it = screen_it.value().find(widget);
    if (widget_it == screen_it.value().end())
        return Preview();
    return widget_it.value();
}

// ScreensModel

ScreensModel::ScreensModel(ColorsModel& colors,
                           ColorRolesModel& roles,
                           FontsModel& fonts,
                           QObject* parent)
    : QAbstractItemModel(parent)
    , m_colorsModel(colors)
    , m_colorRolesModel(roles)
    , m_fontsModel(fonts)
    , m_root(new WidgetData(WidgetData::WidgetType::Widget))
    , m_commander(new QUndoStack(this))
{
    m_commander->setUndoLimit(100);
    m_root->setModel(this);
    connect(&colors, &ColorsModel::valueChanged, this, &ScreensModel::onColorChanged);
    connect(&roles, &ColorRolesModel::colorChanged, this, &ScreensModel::onStyledColorChanged);
    connect(&fonts, &FontsModel::valueChanged, this, &ScreensModel::onFontChanged);
}

ScreensModel::~ScreensModel()
{
    delete m_root;
}
QVariant ScreensModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ColumnElement:
            return "Element";
        case ColumnName:
            return "Name";
        }
    }
    return QVariant();
}

QModelIndex ScreensModel::index(int row, int column, const QModelIndex& parent) const
{
    // check that row and column are in range
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    Item* parentItem = indexToItem(parent);
    Item* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex ScreensModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    Item* item = castItem(index)->parent()->self();
    if (item == m_root)
        return QModelIndex();
    return createIndex(item->myIndex(), ColumnElement, item);
}

int ScreensModel::rowCount(const QModelIndex& parent) const
{
    Item* parentItem = indexToItem(parent);
    return parentItem->childCount();
}

int ScreensModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return ColumnsCount;
}

QVariant ScreensModel::data(const QModelIndex& index, int role) const
{
    using WidgetType = WidgetData::WidgetType;

    if (!index.isValid())
        return QVariant();

    auto* widget = castItem(index);

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        switch (index.column()) {
        case ColumnElement:
            return widget->typeStr();
        case ColumnName:
            switch (widget->type()) {
            case WidgetType::Label:
                return widget->text();
            case WidgetType::Pixmap:
                return widget->getAttr(Property::pixmap).toString();
            case WidgetType::Screen:
            case WidgetType::Panel:
            case WidgetType::Widget:
                if (widget->name().isNull() && !widget->source().isEmpty()) {
                    return widget->source();
                }
                return widget->name();
            case WidgetType::Applet:
                return widget->getAttr("type");
            case WidgetType::Include:
                return widget->getAttr("filename");
            }
        }
    case ScreensModel::TypeRole:
        return static_cast<int>(widget->type());
    case ScreensModel::PanelIndexRole: {
        // TODO: is some sort of cache required?
        auto it = m_root->dfs_begin();
        while (it != m_root->dfs_end()) {
            if (it->type() == WidgetType::Screen) {
                if (it->name() == widget->name()) {
                    return createIndex(it->myIndex(), ColumnElement, it->self());
                } else {
                    it.skip(); // skip this node
                }
            } else {
                ++it; // traverse also children
            }
        }
        return QModelIndex();
    }
    default:
        return QVariant();
    }
}

bool ScreensModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    auto* widget = castItem(index);

    switch (role) {
    case Qt::EditRole:
        switch (index.column()) {
        case ColumnName:
            using WidgetType = WidgetData::WidgetType;
            switch (widget->type()) {
            case WidgetType::Label:
                return setWidgetAttr(index, Property::text, value);
            case WidgetType::Pixmap:
                return setWidgetAttr(index, Property::pixmap, value);
            case WidgetType::Screen:
            case WidgetType::Widget:
            case WidgetType::Panel:
                return setWidgetAttr(index, Property::name, value);
            case WidgetType::Applet:
                return false; // TODO: what to set here?
            }
        default:
            return false;
        }
    default:
        return false;
    }
}

Qt::ItemFlags ScreensModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled; // Drops to the root item

    auto commonFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;

    if (index.column() == ColumnElement) {
        return commonFlags | Qt::ItemIsDropEnabled;
    } else {
        return commonFlags | Qt::ItemIsEditable;
    }
}

bool ScreensModel::insertWidget(int row, const QModelIndex& parent, WidgetData::WidgetType type)
{
    Item* parentItem = indexToItem(parent);
    if (row < 0 || row > rowCount(parent)) {
        return false;
    }

    QVector<WidgetData*> childs{ new WidgetData(type) };
    m_commander->push(new InsertRowsCommand(*parentItem, row, childs));
    return true;
}

bool ScreensModel::insertRows(int row, int count, const QModelIndex& parent)
{
    Item* parentItem = indexToItem(parent);
    if (row < 0 || row > rowCount(parent)) {
        return false;
    }

    using t = WidgetData::WidgetType;
    auto type = t::Widget;
    // Top level items should be Screens
    if (parentItem == m_root) {
        type = t::Screen;
    }
    QVector<WidgetData*> childs;
    for (int i = 0; i < count; ++i) {
        childs.append(new WidgetData(type));
    }
    m_commander->push(new InsertRowsCommand(*parentItem, row, childs));
    return true;
}

bool ScreensModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Item* parentItem = indexToItem(parent);
    if (count <= 0 || row < 0 || row + count > parentItem->childCount()) {
        return false;
    }
    m_commander->push(new RemoveRowsCommand(*parentItem, row, count));
    return true;
}

/// Takes children from parent emitting necessary notifications
QVector<WidgetData*> ScreensModel::takeChildren(int row, int count, WidgetData& parent)
{
    Q_ASSERT(count > 0 && 0 <= row && row + count <= parent.childCount());
    QModelIndex parentIndex;
    // createIndex doesn't work for root node
    if (&parent != m_root) {
        parentIndex = createIndex(parent.myIndex(), ColumnElement, &parent);
    }
    beginRemoveRows(parentIndex, row, row + count - 1);
    auto items = parent.takeChildren(row, count);
    endRemoveRows();
    return items;
}

/// Inserts children into parent emitting necessary notifications
void ScreensModel::insertChildren(int row, const QVector<WidgetData*>& childs, WidgetData& parent)
{
    Q_ASSERT(0 <= row && row <= parent.childCount());
    QModelIndex parentIndex;
    // createIndex doesn't work for root node
    if (&parent != m_root) {
        parentIndex = createIndex(parent.myIndex(), ColumnElement, &parent);
    }
    beginInsertRows(parentIndex, row, row + childs.count() - 1);
    parent.insertChildren(row, childs);
    endInsertRows();
}

void ScreensModel::clear()
{
    beginResetModel();
    QModelIndex rootIndex;
    removeRows(0, rowCount(rootIndex), rootIndex);
    endResetModel();
    m_commander->clear();
}

bool ScreensModel::moveRows(const QModelIndex& sourceParent,
                            int sourceRow,
                            int count,
                            const QModelIndex& destinationParent,
                            int destinationChild)
{
    if (!isValidMove(sourceParent, sourceRow, count, destinationParent, destinationChild)) {
        return false;
    }

    auto* source = indexToItem(sourceParent);
    auto* destination = indexToItem(destinationParent);

    if (beginMoveRows(
          sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild)) {
        auto items = source->takeChildren(sourceRow, count);
        if (source == destination && sourceRow < destinationChild) {
            destinationChild -= count;
        }
        destination->insertChildren(destinationChild, items);
        endMoveRows();
        return true;
    }
    return false;
}

QMimeData* ScreensModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.count() <= 0)
        return nullptr;

    auto types = mimeTypes();
    Q_ASSERT(types.count() > 0);
    QString format = types.at(0);

    // Store list of row indexes in the QMimeData
    QMimeData* data = new QMimeData();
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    encodeRows(indexes, stream);
    data->setData(format, encoded);
    return data;
}

bool ScreensModel::dropMimeData(const QMimeData* data,
                                Qt::DropAction action,
                                int row,
                                int column,
                                const QModelIndex& parent)
{
    if (action == Qt::IgnoreAction)
        return true;
    if (action != Qt::MoveAction)
        return false;

    auto types = mimeTypes();
    Q_ASSERT(types.count() > 0);
    QString format = types.at(0);

    // Decode list of row indexes to move within the model
    Q_UNUSED(column);
    QByteArray encoded = data->data(format);
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    QVector<QModelIndex> rows = decodeRows(stream);

    bool ok = true;
    for (auto index : qAsConst(rows)) {
        ok &= moveRow(index.parent(), index.row(), parent, row);
    }
    if (!ok) {
        qWarning() << "Move rows failed";
    }

    // Default implementation removes successfully moved out rows, return false to disable it.
    return false;
}

Qt::DropActions ScreensModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

void ScreensModel::appendFromXml(QXmlStreamReader& xml)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "screen");

    auto* w = WidgetData::createFromXml(xml);
    if (!w) {
        return;
    }

    beginInsertRows(QModelIndex(), m_root->childCount(), m_root->childCount());
    m_root->appendChild(w);
    w->loadPreview(); // After widget is attached to the model
    endInsertRows();
}

void ScreensModel::appendIncludeFromXml(QXmlStreamReader& xml)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == IncludeFile::tag);

    beginInsertRows(QModelIndex(), m_root->childCount(), m_root->childCount());

    auto* element = new IncludeFile();
    element->fromXml(xml);
    m_root->appendChild(element);
    element->loadPreview(); // After it is attached to the model

    endInsertRows();
}

void ScreensModel::toXml(XmlStreamWriter& xml)
{
    for (int i = 0; i < m_root->childCount(); ++i) {
        m_root->child(i)->toXml(xml);
    }
}

const WidgetData& ScreensModel::widget(const QModelIndex& index) const
{
    if (!index.isValid())
        qWarning() << "You asked for a widget at bad index!";
    return *indexToItem(index);
}

QVariant ScreensModel::widgetAttr(const QModelIndex& index, int key) const
{
    if (!index.isValid())
        return QVariant();

    auto* widget = static_cast<WidgetData*>(index.internalPointer());
    return widget->getAttr(key);
}

bool ScreensModel::setWidgetAttr(const QModelIndex& index, int key, const QVariant& value)
{
    auto* widget = indexToItem(index);
    if (widget) {
        m_commander->push(new AttrCommand(widget, key, value));
        return true;
    }
    return false;
}

bool ScreensModel::setWidgetDataFromXml(const QModelIndex& index, QXmlStreamReader& xml)
{
    if (!index.isValid()) {
        return false;
    }
    auto* widget = WidgetData::createFromXml(xml);
    if (!widget) {
        return false;
    }
    // store preview modification separately, because they are not in xml
    updatePreviewMap(index);

    QModelIndex parent = index.parent();
    m_commander->beginMacro("Edit XML source");
    // Remove old widget
    m_commander->push(new RemoveRowsCommand(*indexToItem(parent), index.row(), 1));
    // Insert constructed widget
    auto child = QVector<WidgetData*>{ widget };
    m_commander->push(new InsertRowsCommand(*indexToItem(parent), index.row(), child));
    m_commander->endMacro();
    // recover preview data
    widget->loadPreview();
    return true;
}

void ScreensModel::resizeWidget(const QModelIndex& index, const QSize& size)
{
    auto* widget = indexToItem(index);
    if (widget) {
        m_commander->push(new ResizeWidgetCommand(widget, size));
    }
}

void ScreensModel::moveWidget(const QModelIndex& index, const QPoint& pos)
{
    auto* widget = indexToItem(index);
    if (widget) {
        m_commander->push(new MoveWidgetCommand(widget, pos));
    }
}

void ScreensModel::changeWidgetRect(const QModelIndex& index, const QRect& rect)
{
    auto* widget = indexToItem(index);
    if (widget) {
        m_commander->push(new ChangeRectWidgetCommand(widget, rect));
    }
}

void ScreensModel::duplicateWdiget(const QModelIndex& index)
{
    auto* widget = indexToItem(index);
    if (widget && widget->parent()) {
        auto child = QVector<WidgetData*>{ widget->clone() };
        m_commander->push(new InsertRowsCommand(*indexToItem(index.parent()), index.row(), child));
    }
}

void ScreensModel::registerObserver(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    if (m_observers[index]++ == 0) {
        indexToItem(index)->updateCache();
    }
}

void ScreensModel::unregisterObserver(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    auto it = m_observers.find(index);
    if (it != m_observers.end()) {
        it.value()--;
        if (it.value() == 0) {
            m_observers.erase(it);
        }
    } else {
        qWarning() << "observers map corrupted";
    }
}

void ScreensModel::widgetAttrHasChanged(const WidgetData* widget, int attrKey)
{
    QModelIndex index =
      createIndex(widget->myIndex(), ColumnElement, const_cast<WidgetData*>(widget));
    emit widgetChanged(index, attrKey);

    //    switch (widget->type()) {
    //    case WidgetData::Label:
    //        if (attrKey != Property::text)
    //            return;
    //        break;
    //    case WidgetData::Pixmap:
    //        if (attrKey != Property::pixmap)
    //            return;
    //        break;
    //    case WidgetData::Screen:
    //    case WidgetData::Widget:
    //        if (attrKey != Property::name)
    //            return;
    //        break;
    //    }
    switch (attrKey) {
    case Property::text:
    case Property::name:
    case Property::source:
    case Property::pixmap:
        auto nameIndex = index.sibling(index.row(), ColumnName);
        emit dataChanged(nameIndex, nameIndex);
    }
}

void ScreensModel::onColorChanged(const QString& name, QRgb value)
{
    for (auto it = m_observers.begin(); it != m_observers.end(); ++it) {
        auto* w = indexToItem(it.key());
        w->onColorChanged(name, value);
    }
}

void ScreensModel::onStyledColorChanged(WindowStyleColor::ColorRole role, QRgb value)
{
    for (auto it = m_observers.begin(); it != m_observers.end(); ++it) {
        auto* w = indexToItem(it.key());
        w->onStyledColorChanged(role, value);
    }
}

void ScreensModel::onFontChanged(const QString& name, const Font& value)
{
    for (auto it = m_observers.begin(); it != m_observers.end(); ++it) {
        auto* w = indexToItem(it.key());
        w->onFontChanged(name, value);
    }
}

ScreensModel::Item* ScreensModel::indexToItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        Q_ASSERT(index.model() == this);
        return static_cast<Item*>(index.internalPointer());
    } else {
        return m_root;
    }
}

ScreensModel::Item* ScreensModel::castItem(const QModelIndex& index)
{
    Q_ASSERT(index.isValid());
    return static_cast<Item*>(index.internalPointer());
}

bool ScreensModel::isValidMove(const QModelIndex& sourceParent,
                               int sourceRow,
                               int count,
                               const QModelIndex& destinationParent,
                               int destinationChild) const
{
    if (sourceRow >= 0 && sourceRow + count <= rowCount(sourceParent) && destinationChild >= 0
        && destinationChild <= rowCount(destinationParent)) {
        bool overlap = (sourceParent == destinationParent)
                       && (destinationChild >= sourceRow && destinationChild <= sourceRow + count);
        return !overlap;
    }
    return false;
}

void ScreensModel::encodeRows(const QModelIndexList& indexes, QDataStream& stream) const
{
    QVector<QModelIndex> rows;
    for (const auto& index : indexes) {
        auto rowIndex = index.sibling(index.row(), 0);
        if (!rows.contains(rowIndex)) {
            rows.append(rowIndex);
        }
    }
    for (const auto& index : rows) {
        stream << index.row() << index.internalId();
    }
}

QVector<QModelIndex> ScreensModel::decodeRows(QDataStream& stream) const
{
    QVector<QModelIndex> rows;
    while (!stream.atEnd()) {
        int r;
        quintptr i;
        stream >> r >> i;
        rows.append(createIndex(r, 0, i));
    }
    return rows;
}

void ScreensModel::updatePreviewMap(const QModelIndex& index)
{
    auto screen = indexToItem(index);
    if (screen) {
        // if it was a widget get a parent screen
        if (screen->type() != WidgetData::WidgetType::Screen) {
            auto ptr = screen->parent();
            if (ptr) {
                screen = ptr->self();
            }
        }
        QString screen_name = screen->name();
        if (screen_name.isEmpty()) {
            return;
        }
        auto& map = m_previews[screen_name];
        for (int i = 0; i < screen->childCount(); ++i) {
            auto w = screen->child(i);
            if (!w->name().isEmpty()) {
                map[w->name()] = Preview(w->previewValue(), w->previewRender());
            }
        }
    }
}

void ScreensModel::savePreviewTree(const QString& path)
{
    // When iterating over QMap order is sorted by name
    // Here walk over the tree structure

    QFile file(path);
    bool ok = file.open(QIODevice::WriteOnly);
    if (!ok)
        return;
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument();
    xml.writeStartElement("screens");
    // iterate over screens
    for (int i = 0; i < m_root->childCount(); ++i) {
        const auto* screen = m_root->child(i);
        xml.writeStartElement("screen");
        xml.writeTextElement("name", screen->name());
        xml.writeStartElement("entries");
        // iterate over widgets
        for (int j = 0; j < screen->childCount(); ++j) {
            auto const* widget = screen->child(j);
            auto valueStr = widget->previewValue().toString();
            auto renderStr = EnumAttr<Property::Render>(widget->previewRender()).toStr();
            if (valueStr.isEmpty())
                continue;
            xml.writeStartElement("entry");
            xml.writeAttribute("name", widget->name());
            xml.writeAttribute("value", valueStr);
            xml.writeAttribute("render", renderStr);
            xml.writeAttribute("type", "string"); // compatibility
            xml.writeEndElement();
        }
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
}

WidgetObserverRegistrator::WidgetObserverRegistrator(ScreensModel* model, const QModelIndex& index)
    : m_model(model)
    , m_index(index)
{
    m_model->registerObserver(index);
}

void WidgetObserverRegistrator::setIndex(const QModelIndex& index)
{
    m_model->unregisterObserver(m_index);
    m_index = index;
    m_model->registerObserver(m_index);
}

WidgetObserverRegistrator::~WidgetObserverRegistrator()
{
    m_model->unregisterObserver(m_index);
}

RemoveRowsCommand::RemoveRowsCommand(WidgetData& root, int row, int count, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_root(root)
    , m_row(row)
    , m_count(count)
{
    // Can not work without a model
    Q_ASSERT(m_root.model() != nullptr);
    setText(QString("rm %1 widgets").arg(count));
}

void RemoveRowsCommand::redo()
{
    // Take ownership from the model
    m_items = m_root.model()->takeChildren(m_row, m_count, m_root);
}

void RemoveRowsCommand::undo()
{
    // Transfer ownership to the model
    m_root.model()->insertChildren(m_row, m_items, m_root);
    m_items.clear();
}

RemoveRowsCommand::~RemoveRowsCommand()
{
    qDeleteAll(m_items);
}

InsertRowsCommand::InsertRowsCommand(WidgetData& root,
                                     int row,
                                     QVector<WidgetData*> items,
                                     QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_root(root)
    , m_row(row)
    , m_count(items.count())
    , m_items(items)
{
    // Can not work without a model
    Q_ASSERT(m_root.model() != nullptr);
    setText(QString("add %1 widgets").arg(items.count()));
}

void InsertRowsCommand::redo()
{
    // Transfer ownership to the model
    m_root.model()->insertChildren(m_row, m_items, m_root);
    m_items.clear();
}

void InsertRowsCommand::undo()
{
    // Take ownership from the model
    m_items = m_root.model()->takeChildren(m_row, m_count, m_root);
}

InsertRowsCommand::~InsertRowsCommand()
{
    qDeleteAll(m_items);
}
