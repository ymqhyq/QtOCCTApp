/****************************************************************************
** Meta object code from reading C++ file 'SARibbonBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../../../../GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonBar.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SARibbonBar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN11SARibbonBarE_t {};
} // unnamed namespace

template <> constexpr inline auto SARibbonBar::qt_create_metaobjectdata<qt_meta_tag_ZN11SARibbonBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SARibbonBar",
        "applicationButtonClicked",
        "",
        "currentRibbonTabChanged",
        "index",
        "ribbonModeChanged",
        "SARibbonBar::RibbonMode",
        "nowState",
        "ribbonStyleChanged",
        "SARibbonBar::RibbonStyles",
        "nowStyle",
        "titleBarHeightChanged",
        "oldHeight",
        "newHeight",
        "actionTriggered",
        "QAction*",
        "action",
        "onWindowTitleChanged",
        "title",
        "onWindowIconChanged",
        "QIcon",
        "i",
        "onCategoryWindowTitleChanged",
        "onStackWidgetHided",
        "onCurrentRibbonTabChanged",
        "onCurrentRibbonTabClicked",
        "onCurrentRibbonTabDoubleClicked",
        "onContextsCategoryPageAdded",
        "SARibbonCategory*",
        "category",
        "onContextsCategoryCategoryNameChanged",
        "onTabMoved",
        "from",
        "to",
        "addCategoryPage",
        "QWidget*",
        "ribbonStyle",
        "RibbonStyles",
        "minimumMode",
        "minimumModeButton",
        "windowTitleTextColor",
        "QColor",
        "tabBarBaseLineColor",
        "windowTitleAligment",
        "Qt::Alignment",
        "enableWordWrap",
        "enableShowPanelTitle",
        "tabOnTitle",
        "panelLayoutMode",
        "SARibbonPanel::PanelLayoutMode",
        "RibbonStyleFlag",
        "RibbonStyleLoose",
        "RibbonStyleCompact",
        "RibbonStyleThreeRow",
        "RibbonStyleTwoRow",
        "RibbonStyleLooseThreeRow",
        "RibbonStyleCompactThreeRow",
        "RibbonStyleLooseTwoRow",
        "RibbonStyleCompactTwoRow",
        "RibbonMode",
        "MinimumRibbonMode",
        "NormalRibbonMode"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'applicationButtonClicked'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'currentRibbonTabChanged'
        QtMocHelpers::SignalData<void(int)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Signal 'ribbonModeChanged'
        QtMocHelpers::SignalData<void(SARibbonBar::RibbonMode)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'ribbonStyleChanged'
        QtMocHelpers::SignalData<void(SARibbonBar::RibbonStyles)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Signal 'titleBarHeightChanged'
        QtMocHelpers::SignalData<void(int, int)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 }, { QMetaType::Int, 13 },
        }}),
        // Signal 'actionTriggered'
        QtMocHelpers::SignalData<void(QAction *)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 15, 16 },
        }}),
        // Slot 'onWindowTitleChanged'
        QtMocHelpers::SlotData<void(const QString &)>(17, 2, QMC::AccessProtected, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Slot 'onWindowIconChanged'
        QtMocHelpers::SlotData<void(const QIcon &)>(19, 2, QMC::AccessProtected, QMetaType::Void, {{
            { 0x80000000 | 20, 21 },
        }}),
        // Slot 'onCategoryWindowTitleChanged'
        QtMocHelpers::SlotData<void(const QString &)>(22, 2, QMC::AccessProtected, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Slot 'onStackWidgetHided'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'onCurrentRibbonTabChanged'
        QtMocHelpers::SlotData<void(int)>(24, 2, QMC::AccessProtected, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Slot 'onCurrentRibbonTabClicked'
        QtMocHelpers::SlotData<void(int)>(25, 2, QMC::AccessProtected, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Slot 'onCurrentRibbonTabDoubleClicked'
        QtMocHelpers::SlotData<void(int)>(26, 2, QMC::AccessProtected, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Slot 'onContextsCategoryPageAdded'
        QtMocHelpers::SlotData<void(SARibbonCategory *)>(27, 2, QMC::AccessProtected, QMetaType::Void, {{
            { 0x80000000 | 28, 29 },
        }}),
        // Slot 'onContextsCategoryCategoryNameChanged'
        QtMocHelpers::SlotData<void(SARibbonCategory *, const QString &)>(30, 2, QMC::AccessProtected, QMetaType::Void, {{
            { 0x80000000 | 28, 29 }, { QMetaType::QString, 18 },
        }}),
        // Slot 'onTabMoved'
        QtMocHelpers::SlotData<void(int, int)>(31, 2, QMC::AccessProtected, QMetaType::Void, {{
            { QMetaType::Int, 32 }, { QMetaType::Int, 33 },
        }}),
        // Method 'addCategoryPage'
        QtMocHelpers::MethodData<void(QWidget *)>(34, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 35, 29 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'ribbonStyle'
        QtMocHelpers::PropertyData<RibbonStyles>(36, 0x80000000 | 37, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
        // property 'minimumMode'
        QtMocHelpers::PropertyData<bool>(38, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'minimumModeButton'
        QtMocHelpers::PropertyData<bool>(39, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable),
        // property 'windowTitleTextColor'
        QtMocHelpers::PropertyData<QColor>(40, 0x80000000 | 41, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
        // property 'tabBarBaseLineColor'
        QtMocHelpers::PropertyData<QColor>(42, 0x80000000 | 41, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
        // property 'windowTitleAligment'
        QtMocHelpers::PropertyData<Qt::Alignment>(43, 0x80000000 | 44, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
        // property 'enableWordWrap'
        QtMocHelpers::PropertyData<bool>(45, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'enableShowPanelTitle'
        QtMocHelpers::PropertyData<bool>(46, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'tabOnTitle'
        QtMocHelpers::PropertyData<bool>(47, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'panelLayoutMode'
        QtMocHelpers::PropertyData<SARibbonPanel::PanelLayoutMode>(48, 0x80000000 | 49, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'RibbonStyleFlag'
        QtMocHelpers::EnumData<enum RibbonStyleFlag>(50, 50, QMC::EnumFlags{}).add({
            {   51, RibbonStyleFlag::RibbonStyleLoose },
            {   52, RibbonStyleFlag::RibbonStyleCompact },
            {   53, RibbonStyleFlag::RibbonStyleThreeRow },
            {   54, RibbonStyleFlag::RibbonStyleTwoRow },
            {   55, RibbonStyleFlag::RibbonStyleLooseThreeRow },
            {   56, RibbonStyleFlag::RibbonStyleCompactThreeRow },
            {   57, RibbonStyleFlag::RibbonStyleLooseTwoRow },
            {   58, RibbonStyleFlag::RibbonStyleCompactTwoRow },
        }),
        // flag 'RibbonStyles'
        QtMocHelpers::EnumData<RibbonStyles>(37, 50, QMC::EnumIsFlag).add({
            {   51, RibbonStyleFlag::RibbonStyleLoose },
            {   52, RibbonStyleFlag::RibbonStyleCompact },
            {   53, RibbonStyleFlag::RibbonStyleThreeRow },
            {   54, RibbonStyleFlag::RibbonStyleTwoRow },
            {   55, RibbonStyleFlag::RibbonStyleLooseThreeRow },
            {   56, RibbonStyleFlag::RibbonStyleCompactThreeRow },
            {   57, RibbonStyleFlag::RibbonStyleLooseTwoRow },
            {   58, RibbonStyleFlag::RibbonStyleCompactTwoRow },
        }),
        // enum 'RibbonMode'
        QtMocHelpers::EnumData<enum RibbonMode>(59, 59, QMC::EnumFlags{}).add({
            {   60, RibbonMode::MinimumRibbonMode },
            {   61, RibbonMode::NormalRibbonMode },
        }),
    };
    return QtMocHelpers::metaObjectData<SARibbonBar, qt_meta_tag_ZN11SARibbonBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT static const QMetaObject::SuperData qt_meta_extradata_ZN11SARibbonBarE[] = {
    QMetaObject::SuperData::link<SARibbonPanel::staticMetaObject>(),
    nullptr
};

Q_CONSTINIT const QMetaObject SARibbonBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QMenuBar::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SARibbonBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SARibbonBarE_t>.data,
    qt_static_metacall,
    qt_meta_extradata_ZN11SARibbonBarE,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11SARibbonBarE_t>.metaTypes,
    nullptr
} };

void SARibbonBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SARibbonBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->applicationButtonClicked(); break;
        case 1: _t->currentRibbonTabChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->ribbonModeChanged((*reinterpret_cast<std::add_pointer_t<SARibbonBar::RibbonMode>>(_a[1]))); break;
        case 3: _t->ribbonStyleChanged((*reinterpret_cast<std::add_pointer_t<SARibbonBar::RibbonStyles>>(_a[1]))); break;
        case 4: _t->titleBarHeightChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->actionTriggered((*reinterpret_cast<std::add_pointer_t<QAction*>>(_a[1]))); break;
        case 6: _t->onWindowTitleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->onWindowIconChanged((*reinterpret_cast<std::add_pointer_t<QIcon>>(_a[1]))); break;
        case 8: _t->onCategoryWindowTitleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->onStackWidgetHided(); break;
        case 10: _t->onCurrentRibbonTabChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->onCurrentRibbonTabClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->onCurrentRibbonTabDoubleClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->onContextsCategoryPageAdded((*reinterpret_cast<std::add_pointer_t<SARibbonCategory*>>(_a[1]))); break;
        case 14: _t->onContextsCategoryCategoryNameChanged((*reinterpret_cast<std::add_pointer_t<SARibbonCategory*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 15: _t->onTabMoved((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 16: _t->addCategoryPage((*reinterpret_cast<std::add_pointer_t<QWidget*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAction* >(); break;
            }
            break;
        case 13:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< SARibbonCategory* >(); break;
            }
            break;
        case 14:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< SARibbonCategory* >(); break;
            }
            break;
        case 16:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QWidget* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SARibbonBar::*)()>(_a, &SARibbonBar::applicationButtonClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonBar::*)(int )>(_a, &SARibbonBar::currentRibbonTabChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonBar::*)(SARibbonBar::RibbonMode )>(_a, &SARibbonBar::ribbonModeChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonBar::*)(SARibbonBar::RibbonStyles )>(_a, &SARibbonBar::ribbonStyleChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonBar::*)(int , int )>(_a, &SARibbonBar::titleBarHeightChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonBar::*)(QAction * )>(_a, &SARibbonBar::actionTriggered, 5))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: QtMocHelpers::assignFlags<RibbonStyles>(_v, _t->currentRibbonStyle()); break;
        case 1: *reinterpret_cast<bool*>(_v) = _t->isMinimumMode(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->haveShowMinimumModeButton(); break;
        case 3: *reinterpret_cast<QColor*>(_v) = _t->windowTitleTextColor(); break;
        case 4: *reinterpret_cast<QColor*>(_v) = _t->tabBarBaseLineColor(); break;
        case 5: *reinterpret_cast<Qt::Alignment*>(_v) = _t->windowTitleAligment(); break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->isEnableWordWrap(); break;
        case 7: *reinterpret_cast<bool*>(_v) = _t->isEnableShowPanelTitle(); break;
        case 8: *reinterpret_cast<bool*>(_v) = _t->isTabOnTitle(); break;
        case 9: *reinterpret_cast<SARibbonPanel::PanelLayoutMode*>(_v) = _t->panelLayoutMode(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setRibbonStyle(*reinterpret_cast<RibbonStyles*>(_v)); break;
        case 1: _t->setMinimumMode(*reinterpret_cast<bool*>(_v)); break;
        case 2: _t->showMinimumModeButton(*reinterpret_cast<bool*>(_v)); break;
        case 3: _t->setWindowTitleTextColor(*reinterpret_cast<QColor*>(_v)); break;
        case 4: _t->setTabBarBaseLineColor(*reinterpret_cast<QColor*>(_v)); break;
        case 5: _t->setWindowTitleAligment(*reinterpret_cast<Qt::Alignment*>(_v)); break;
        case 6: _t->setEnableWordWrap(*reinterpret_cast<bool*>(_v)); break;
        case 7: _t->setEnableShowPanelTitle(*reinterpret_cast<bool*>(_v)); break;
        case 8: _t->setTabOnTitle(*reinterpret_cast<bool*>(_v)); break;
        case 9: _t->setPanelLayoutMode(*reinterpret_cast<SARibbonPanel::PanelLayoutMode*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *SARibbonBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SARibbonBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SARibbonBarE_t>.strings))
        return static_cast<void*>(this);
    return QMenuBar::qt_metacast(_clname);
}

int SARibbonBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMenuBar::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void SARibbonBar::applicationButtonClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void SARibbonBar::currentRibbonTabChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void SARibbonBar::ribbonModeChanged(SARibbonBar::RibbonMode _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SARibbonBar::ribbonStyleChanged(SARibbonBar::RibbonStyles _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void SARibbonBar::titleBarHeightChanged(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void SARibbonBar::actionTriggered(QAction * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
