#ifndef ENUMS_HPP
#define ENUMS_HPP


#include <QMetaEnum>
#include <QString>

/**
 * @brief Dummy class to store properties enum
 * Q_GADGET allows Qt features such as string/enum conversion
 */
class Property : public QObject
{
    // for enum
    Q_GADGET
public:
    enum PropertyEnum {
        // key not assigned
        invalid = -1,
        // widget
        name,
        position,
        size,
        zPosition,
        transparent,
        borderColor,
        borderWidth,
        // pixmap
        pixmap,
        alphatest,
        scale,
        // label
        text,
        font,
        valign,
        halign,
        shadowColor,
        shadowOffset,
        // screen
        title,
        flags,
        // list
        itemHeight,
        selectionPixmap,
        selectionDisabled,
        scrollbarMode,
        enableWrapAround,
        // slider
        sliderPixmap,
        backgroundPixmap,
        orientation,
        // general
        backgroundColor,
        backgroundColorSelected,
        foregroundColor,
        foregroundColorSelected,
        // position gauge
        pointer,
        seek_pointer,
        // widget
        render,
        source,
        // below goes our preview extension
        preview,
        previewRender,
        previewValue,
    };
    Q_ENUM(PropertyEnum)

    enum Alphatest {
        off,
        on,
        blend,
    };
    Q_ENUM(Alphatest)

    enum Orientation {
        orHorizontal,
        orVertical,
    };
    Q_ENUM(Orientation)

    enum ScrollbarMode {
        showNever,
        showOnDemand,
        showAlways,
        showLeft,
    };
    Q_ENUM(ScrollbarMode)

    enum Render {
        Widget,
        Screen,
        Label,
        Pixmap,
        Slider,
    };
    Q_ENUM(Render)

    enum Flags {
        wfBorder,
        wfNoBorder,
    };
    Q_ENUM(Flags)

    enum BorderSet {
        bsWindow,
        bsButton,
        bsListboxEntry,
    };
    Q_ENUM(BorderSet)

    enum BorderPosition {
        bpTopLeft,
        bpTop,
        bpTopRight,
        bpLeft,
        bpRight,
        bpBottomLeft,
        bpBottom,
        bpBottomRight,
    };
    Q_ENUM(BorderPosition)

    static QMetaEnum& propertyEnum()
    {
        static QMetaEnum metaEnum = QMetaEnum::fromType<Property::PropertyEnum>();
        return metaEnum;
    }
};

// separate this because of namespace conflict
class PropertyVAlign : public QObject
{
    // for enum
    Q_GADGET
public:
    enum Enum {
        top = Qt::AlignTop,
        bottom = Qt::AlignBottom,
        center = Qt::AlignVCenter,
    };
    Q_ENUM(Enum)
};
class PropertyHAlign : public QObject
{
    // for enum
    Q_GADGET
public:
    enum Enum {
        left = Qt::AlignLeft,
        right = Qt::AlignRight,
        center = Qt::AlignHCenter,
    };
    Q_ENUM(Enum)
};

// Roles to be used in QtModels

enum Roles {
    UserRole = Qt::UserRole,
    // Roles for scene
    GraphicsRole,
    AnchorRole,
    // Role for QComboxBox
    StringListRole,
    //	// Role to select editor
    //	RoleDelegate, // TODO
    // Role to get type()
    WidgetTypeRole,
    // Role to store in XML
    XmlRole,
    DataRole,
};


#endif // ENUMS_HPP
