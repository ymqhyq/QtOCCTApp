/****************************************************************************
** Meta object code from reading C++ file 'SARibbonContextCategory.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../../../../GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonContextCategory.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SARibbonContextCategory.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23SARibbonContextCategoryE_t {};
} // unnamed namespace

template <> constexpr inline auto SARibbonContextCategory::qt_create_metaobjectdata<qt_meta_tag_ZN23SARibbonContextCategoryE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SARibbonContextCategory",
        "categoryPageAdded",
        "",
        "SARibbonCategory*",
        "category",
        "categoryPageRemoved",
        "contextTitleChanged",
        "title",
        "categoryTitleChanged",
        "hide",
        "show",
        "onCategoryTitleChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'categoryPageAdded'
        QtMocHelpers::SignalData<void(SARibbonCategory *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'categoryPageRemoved'
        QtMocHelpers::SignalData<void(SARibbonCategory *)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'contextTitleChanged'
        QtMocHelpers::SignalData<void(const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Signal 'categoryTitleChanged'
        QtMocHelpers::SignalData<void(SARibbonCategory *, const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 7 },
        }}),
        // Slot 'hide'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'show'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onCategoryTitleChanged'
        QtMocHelpers::SlotData<void(const QString &)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SARibbonContextCategory, qt_meta_tag_ZN23SARibbonContextCategoryE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SARibbonContextCategory::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SARibbonContextCategoryE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SARibbonContextCategoryE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23SARibbonContextCategoryE_t>.metaTypes,
    nullptr
} };

void SARibbonContextCategory::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SARibbonContextCategory *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->categoryPageAdded((*reinterpret_cast<std::add_pointer_t<SARibbonCategory*>>(_a[1]))); break;
        case 1: _t->categoryPageRemoved((*reinterpret_cast<std::add_pointer_t<SARibbonCategory*>>(_a[1]))); break;
        case 2: _t->contextTitleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->categoryTitleChanged((*reinterpret_cast<std::add_pointer_t<SARibbonCategory*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->hide(); break;
        case 5: _t->show(); break;
        case 6: _t->onCategoryTitleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< SARibbonCategory* >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< SARibbonCategory* >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< SARibbonCategory* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SARibbonContextCategory::*)(SARibbonCategory * )>(_a, &SARibbonContextCategory::categoryPageAdded, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonContextCategory::*)(SARibbonCategory * )>(_a, &SARibbonContextCategory::categoryPageRemoved, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonContextCategory::*)(const QString & )>(_a, &SARibbonContextCategory::contextTitleChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SARibbonContextCategory::*)(SARibbonCategory * , const QString & )>(_a, &SARibbonContextCategory::categoryTitleChanged, 3))
            return;
    }
}

const QMetaObject *SARibbonContextCategory::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SARibbonContextCategory::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SARibbonContextCategoryE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SARibbonContextCategory::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void SARibbonContextCategory::categoryPageAdded(SARibbonCategory * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SARibbonContextCategory::categoryPageRemoved(SARibbonCategory * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void SARibbonContextCategory::contextTitleChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SARibbonContextCategory::categoryTitleChanged(SARibbonCategory * _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}
QT_WARNING_POP
