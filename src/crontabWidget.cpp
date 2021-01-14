/*
    KT main GUI view implementation
    --------------------------------------------------------------------
    SPDX-FileCopyrightText: 1999 Gary Meyer <gary@meyer.net>
    --------------------------------------------------------------------
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "crontabWidget.h"

#include <stdlib.h>
#include <unistd.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QApplication>
#include <QClipboard>

#include <KLocalizedString>
#include <QIcon>
#include <QAction>
#include <KStandardAction>

#include "cthost.h"
#include "ctcron.h"
#include "ctvariable.h"
#include "cttask.h"
#include "ctGlobalCron.h"

#include "crontabPrinter.h"
#include "taskWidget.h"

#include "variableWidget.h"

#include "kcmCron.h"

#include "logging.h"

class CTGlobalCron;

class CrontabWidgetPrivate {
public:

	/**
	 * The application.
	 */
    CTHost* ctHost = nullptr;

	/**
	 * Tree view of the crontab tasks.
	 */
    TasksWidget* tasksWidget = nullptr;

	/**
	 * Tree view of the crontab tasks.
	 */
    VariablesWidget* variablesWidget = nullptr;

    QAction* cutAction = nullptr;
    QAction* copyAction = nullptr;
    QAction* pasteAction = nullptr;

	/**
	 * Clipboard tasks.
	 */
	QList<CTTask*> clipboardTasks;

	/**
	 * Clipboard variable.
	 */
	QList<CTVariable*> clipboardVariables;

    QRadioButton* currentUserCronRadio = nullptr;
    QRadioButton* systemCronRadio = nullptr;
    QRadioButton* otherUserCronRadio = nullptr;

    QComboBox* otherUsers = nullptr;

	/**
	 * Pointer to the pseudo Global Cron object
	 */
    CTGlobalCron* ctGlobalCron = nullptr;


};

CrontabWidget::CrontabWidget(QWidget* parent, CTHost* ctHost) :
	QWidget(parent), d(new CrontabWidgetPrivate()) {

	d->ctHost = ctHost;

	if (d->ctHost->isRootUser()) {
		d->ctGlobalCron = new CTGlobalCron(d->ctHost);
	}
	else {
		d->ctGlobalCron = nullptr;
	}

	setupActions();

	initialize();

	logDebug() << "Clipboard Status " << hasClipboardContent();

	d->tasksWidget->setFocus();

	QTreeWidgetItem* item = d->tasksWidget->treeWidget()->topLevelItem(0);
	if (item != nullptr) {
		logDebug() << "First item found" << d->tasksWidget->treeWidget()->topLevelItemCount();
		item->setSelected(true);
	}

	d->tasksWidget->changeCurrentSelection();
	d->variablesWidget->changeCurrentSelection();

}

CrontabWidget::~CrontabWidget() {
	delete d->tasksWidget;
	delete d->variablesWidget;

	delete d->ctGlobalCron;

	delete d;
}

bool CrontabWidget::hasClipboardContent() {
    if (!d->clipboardTasks.isEmpty())
		return true;

    if (!d->clipboardVariables.isEmpty())
		return true;

	return false;
}

QHBoxLayout* CrontabWidget::createCronSelector() {
	QHBoxLayout* layout = new QHBoxLayout();

	layout->addWidget(new QLabel(i18n("Show the following Cron:"), this));

	QButtonGroup* group = new QButtonGroup(this);

	d->currentUserCronRadio = new QRadioButton(i18n("Personal Cron"), this);
	d->currentUserCronRadio->setChecked(true);
	group->addButton(d->currentUserCronRadio);
	layout->addWidget(d->currentUserCronRadio);

	d->systemCronRadio = new QRadioButton(i18n("System Cron"), this);
	group->addButton(d->systemCronRadio);
	layout->addWidget(d->systemCronRadio);

	d->otherUserCronRadio = new QRadioButton(i18n("Cron of User:"), this);
	group->addButton(d->otherUserCronRadio);

	d->otherUsers = new QComboBox(this);

	layout->addWidget(d->otherUserCronRadio);
	layout->addWidget(d->otherUsers);

	if (ctHost()->isRootUser()) {
		QStringList users;

        const auto crons = ctHost()->crons;
        for (CTCron* ctCron : crons) {
			if (ctCron->isCurrentUserCron())
				continue;

			if (ctCron->isSystemCron())
				continue;

			users.append(ctCron->userLogin());
		}

		users.sort();
		d->otherUsers->addItems(users);
		d->otherUsers->addItem(QIcon::fromTheme( QStringLiteral( "users") ), i18n("Show All Personal Crons"));
	} else {
		d->otherUserCronRadio->hide();
		d->otherUsers->hide();
	}

	connect(group, static_cast<void (QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked), this, &CrontabWidget::refreshCron);
	connect(d->otherUsers, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CrontabWidget::checkOtherUsers);

	layout->addStretch(1);

	return layout;
}

void CrontabWidget::initialize() {
	QVBoxLayout* layout = new QVBoxLayout(this);

	logDebug() << "Begin view refresh";

	logDebug() << "Creating Tasks list...";

	QHBoxLayout* cronSelector = createCronSelector();
	layout->addLayout(cronSelector);

	QSplitter* splitter = new QSplitter(this);
	splitter->setOrientation(Qt::Vertical);
	layout->addWidget(splitter);

	d->tasksWidget = new TasksWidget(this);
	splitter->addWidget(d->tasksWidget);
	splitter->setStretchFactor(0, 2);

	d->variablesWidget = new VariablesWidget(this);
	splitter->addWidget(d->variablesWidget);
	splitter->setStretchFactor(1, 1);

	refreshCron();

}

void CrontabWidget::refreshCron() {

	CTCron* ctCron = currentCron();

	d->tasksWidget->refreshTasks(ctCron);
	d->variablesWidget->refreshVariables(ctCron);

	if (ctCron->isMultiUserCron() && ctHost()->isRootUser()==false) {
		logDebug() << "Disabling view...";

		d->tasksWidget->treeWidget()->setEnabled(false);
		d->variablesWidget->treeWidget()->setEnabled(false);

		toggleNewEntryActions(false);
		toggleModificationActions(false);
		togglePasteAction(false);
		d->tasksWidget->toggleRunNowAction(false);
	}
	else {
		logDebug() << "Enabling view...";

		d->tasksWidget->treeWidget()->setEnabled(true);
		d->variablesWidget->treeWidget()->setEnabled(true);

		toggleNewEntryActions(true);
		togglePasteAction(hasClipboardContent());
	}
}


void CrontabWidget::copy() {
	foreach(CTTask* task, d->clipboardTasks) {
		delete task;
	}
	d->clipboardTasks.clear();

	foreach(CTVariable* variable, d->clipboardVariables) {
		delete variable;
	}
	d->clipboardVariables.clear();

	QString clipboardText;

	if (d->tasksWidget->treeWidget()->hasFocus()) {
		logDebug() << "Tasks copying";

        const QList<TaskWidget*> tasksWidget = d->tasksWidget->selectedTasksWidget();
        for (TaskWidget* taskWidget : tasksWidget) {
			CTTask* task = new CTTask( *(taskWidget->getCTTask()) );
			d->clipboardTasks.append(task);

			clipboardText += task->exportTask() + QLatin1String( "\n" );
		}
	}

	if (d->variablesWidget->treeWidget()->hasFocus()) {
		logDebug() << "Variables copying";

		QList<VariableWidget*> variablesWidget = d->variablesWidget->selectedVariablesWidget();
		foreach(VariableWidget* variableWidget, variablesWidget) {
			CTVariable* variable = new CTVariable( *(variableWidget->getCTVariable()) );
			d->clipboardVariables.append(variable);

			clipboardText += variable->exportVariable() + QLatin1String( "\n" );
		}
	}

	QApplication::clipboard()->setText(clipboardText, QClipboard::Clipboard);
	QApplication::clipboard()->setText(clipboardText, QClipboard::Selection);

	logDebug() << "Clipboard Status " << hasClipboardContent();
	togglePasteAction(hasClipboardContent());
}

void CrontabWidget::cut() {
	logDebug() << "Cut content";

	copy();

	if (d->tasksWidget->treeWidget()->hasFocus()) {
		logDebug() << "Tasks cutting";
		d->tasksWidget->deleteSelection();
	}

	if (d->variablesWidget->treeWidget()->hasFocus()) {
		logDebug() << "Variables cutting";
		d->variablesWidget->deleteSelection();
	}
}

void CrontabWidget::paste() {
	logDebug() << "Paste content";

	if (d->tasksWidget->treeWidget()->hasFocus()) {
		foreach(CTTask* task, d->clipboardTasks) {
			d->tasksWidget->addTask(new CTTask(*task));
		}
	}

	if (d->variablesWidget->treeWidget()->hasFocus()) {
		foreach(CTVariable* variable, d->clipboardVariables) {
			d->variablesWidget->addVariable(new CTVariable(*variable));
		}
	}

}

CTCron* CrontabWidget::currentCron() const {
	if (d->currentUserCronRadio->isChecked())
		return d->ctHost->findCurrentUserCron();
	else if (d->systemCronRadio->isChecked())
		return d->ctHost->findSystemCron();

	if (d->otherUsers->currentIndex() == d->otherUsers->count()-1) {
		logDebug() << "Using Global Cron";
		return d->ctGlobalCron;
	}

	QString currentUserLogin = d->otherUsers->currentText();
	return d->ctHost->findUserCron(currentUserLogin);
}

TasksWidget* CrontabWidget::tasksWidget() const {
	return d->tasksWidget;
}

VariablesWidget* CrontabWidget::variablesWidget() const {
	return d->variablesWidget;
}

CTHost* CrontabWidget::ctHost() const {
	return d->ctHost;
}

void CrontabWidget::checkOtherUsers() {
	d->otherUserCronRadio->setChecked(true);

	refreshCron();
}

void CrontabWidget::setupActions() {
	logDebug() << "Setup actions";

	//Edit menu
	d->cutAction = KStandardAction::cut(this, SLOT(cut()), this);
	d->copyAction = KStandardAction::copy(this, SLOT(copy()), this);
	d->pasteAction = KStandardAction::paste(this, SLOT(paste()), this);
	togglePasteAction(false);

	logDebug() << "Actions initialized";

}


QList<QAction*> CrontabWidget::cutCopyPasteActions() {
	QList<QAction*> actions;
	actions.append(d->cutAction);
	actions.append(d->copyAction);
	actions.append(d->pasteAction);

	return actions;
}

void CrontabWidget::togglePasteAction(bool state) {
	d->pasteAction->setEnabled(state);
}

void CrontabWidget::toggleModificationActions(bool state) {
	d->cutAction->setEnabled(state);
	d->copyAction->setEnabled(state);

	d->tasksWidget->toggleModificationActions(state);
	d->variablesWidget->toggleModificationActions(state);
}

void CrontabWidget::toggleNewEntryActions(bool state) {
	d->tasksWidget->toggleNewEntryAction(state);
	d->variablesWidget->toggleNewEntryAction(state);
}

void CrontabWidget::print() {

	CrontabPrinter printer(this);

	if (printer.start() == false) {
		logDebug() << "Unable to start printer";
		return;
	}
	printer.printTasks();
	printer.printVariables();

	printer.finish();

}


