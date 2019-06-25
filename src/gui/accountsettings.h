/*
 * NetVirt - Network Virtualization Platform
 * Copyright (C) 2009-2014
 * Nicolas J. Bouliane <admin@netvirt.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef ACCOUNTSETTINGS_H
#define ACCOUNTSETTINGS_H

#include <QDialog>
#include <QString>
#include <QThread>

#include "maindialog.h"
#include "ui_accountsettings.h"

#include "../agent.h"

class Worker : public QThread
{
	Q_OBJECT

public:
	QString networkName;

protected:
	void run();
};

class AccountSettings: public QDialog
{
	Q_OBJECT

	public:
		AccountSettings(MainDialog *dialog);
		virtual ~AccountSettings();

	public slots:
		void slotFireConnection();
		void slotOnConnect(QString ip);
		void slotConnWaiting();
		void slotListNetworks(QString network);

	private:
		Ui::AccountSettings ui;
		QMovie *movie;
};

#endif // ACCOUNTSETTINGS_H
