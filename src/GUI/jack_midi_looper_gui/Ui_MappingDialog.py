# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'MappingDialog.ui'
#
# Created: Fri Aug 29 13:07:02 2014
#      by: PyQt5 UI code generator 5.3.1
#
# WARNING! All changes made in this file will be lost!

from PyQt5 import QtCore, QtGui, QtWidgets

class Ui_MappingDialog(object):
    def setupUi(self, MappingDialog):
        MappingDialog.setObjectName("MappingDialog")
        MappingDialog.resize(477, 156)
        self.buttonBox = QtWidgets.QDialogButtonBox(MappingDialog)
        self.buttonBox.setGeometry(QtCore.QRect(270, 120, 181, 32))
        self.buttonBox.setOrientation(QtCore.Qt.Horizontal)
        self.buttonBox.setStandardButtons(QtWidgets.QDialogButtonBox.Cancel|QtWidgets.QDialogButtonBox.Ok)
        self.buttonBox.setObjectName("buttonBox")
        self.spinBox_channel = QtWidgets.QSpinBox(MappingDialog)
        self.spinBox_channel.setGeometry(QtCore.QRect(120, 10, 60, 31))
        self.spinBox_channel.setMinimum(1)
        self.spinBox_channel.setMaximum(16)
        self.spinBox_channel.setObjectName("spinBox_channel")
        self.label = QtWidgets.QLabel(MappingDialog)
        self.label.setGeometry(QtCore.QRect(20, 10, 91, 31))
        self.label.setObjectName("label")
        self.spinBox_midi_value = QtWidgets.QSpinBox(MappingDialog)
        self.spinBox_midi_value.setGeometry(QtCore.QRect(120, 90, 60, 31))
        self.spinBox_midi_value.setMinimum(0)
        self.spinBox_midi_value.setMaximum(127)
        self.spinBox_midi_value.setObjectName("spinBox_midi_value")
        self.label_2 = QtWidgets.QLabel(MappingDialog)
        self.label_2.setGeometry(QtCore.QRect(20, 90, 91, 31))
        self.label_2.setObjectName("label_2")
        self.label_3 = QtWidgets.QLabel(MappingDialog)
        self.label_3.setGeometry(QtCore.QRect(20, 50, 91, 31))
        self.label_3.setObjectName("label_3")
        self.comboBox_type = QtWidgets.QComboBox(MappingDialog)
        self.comboBox_type.setGeometry(QtCore.QRect(120, 50, 91, 27))
        self.comboBox_type.setObjectName("comboBox_type")
        self.comboBox_type.addItem("")
        self.comboBox_type.addItem("")
        self.comboBox_type.addItem("")
        self.comboBox_type.addItem("")
        self.comboBox_loop = QtWidgets.QComboBox(MappingDialog)
        self.comboBox_loop.setGeometry(QtCore.QRect(280, 10, 181, 27))
        self.comboBox_loop.setObjectName("comboBox_loop")
        self.label_4 = QtWidgets.QLabel(MappingDialog)
        self.label_4.setGeometry(QtCore.QRect(240, 10, 51, 31))
        self.label_4.setObjectName("label_4")
        self.comboBox_action = QtWidgets.QComboBox(MappingDialog)
        self.comboBox_action.setGeometry(QtCore.QRect(290, 50, 171, 27))
        self.comboBox_action.setObjectName("comboBox_action")
        self.comboBox_action.addItem("")
        self.comboBox_action.addItem("")
        self.label_5 = QtWidgets.QLabel(MappingDialog)
        self.label_5.setGeometry(QtCore.QRect(240, 50, 71, 31))
        self.label_5.setObjectName("label_5")

        self.retranslateUi(MappingDialog)
        self.buttonBox.accepted.connect(MappingDialog.accept)
        self.buttonBox.rejected.connect(MappingDialog.reject)
        QtCore.QMetaObject.connectSlotsByName(MappingDialog)

    def retranslateUi(self, MappingDialog):
        _translate = QtCore.QCoreApplication.translate
        MappingDialog.setWindowTitle(_translate("MappingDialog", "New Mapping"))
        self.label.setText(_translate("MappingDialog", "MIDI Channel"))
        self.label_2.setText(_translate("MappingDialog", "MIDI Value"))
        self.label_3.setText(_translate("MappingDialog", "MIDI Type"))
        self.comboBox_type.setItemText(0, _translate("MappingDialog", "Note On"))
        self.comboBox_type.setItemText(1, _translate("MappingDialog", "Note Off"))
        self.comboBox_type.setItemText(2, _translate("MappingDialog", "CC On"))
        self.comboBox_type.setItemText(3, _translate("MappingDialog", "CC Off"))
        self.label_4.setText(_translate("MappingDialog", "Loop"))
        self.comboBox_action.setItemText(0, _translate("MappingDialog", "Toggle Recording"))
        self.comboBox_action.setItemText(1, _translate("MappingDialog", "Toggle Playback"))
        self.label_5.setText(_translate("MappingDialog", "Action"))

