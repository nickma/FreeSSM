/*
 * Engine.cpp - Engine Control Unit dialog
 *
 * Copyright (C) 2008-2018 Comer352L
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "EngineDialog.h"


EngineDialog::EngineDialog(AbstractDiagInterface *diagInterface, QString language) : ControlUnitDialog(tr("Engine Control Unit"), diagInterface, language)
{
	// *** Initialize global variables:
	_content_DCs = NULL;
	_content_MBsSWs = NULL;
	_content_Adjustments = NULL;
	_content_SysTests = NULL;
	// Show information-widget:
	_infoWidget = new CUinfo_Engine();
	setInfoWidget(_infoWidget);
	_infoWidget->show();
	// Setup functions:
	QPushButton *pushButton = addContent(ContentSelection::DCsMode);
	connect( pushButton, SIGNAL( clicked() ), this, SLOT( switchToDCsMode() ) );
	pushButton = addContent(ContentSelection::MBsSWsMode);
	connect( pushButton, SIGNAL( clicked() ), this, SLOT( switchToMBsSWsMode() ) );
	pushButton = addContent(ContentSelection::AdjustmentsMode);
	connect( pushButton, SIGNAL( clicked() ), this, SLOT( switchToAdjustmentsMode() ) );
	pushButton = addContent(ContentSelection::SysTestsMode);
	connect( pushButton, SIGNAL( clicked() ), this, SLOT( switchToSystemOperationTestsMode() ) );
	pushButton = addContent(ContentSelection::ClearMemoryFcn);
	connect( pushButton, SIGNAL( clicked() ), this, SLOT( clearMemory() ) );
}


bool EngineDialog::setup(ContentSelection csel)
{
	QString sysdescription = "";
	std::string SYS_ID = "";
	std::string ROM_ID = "";

	if (_setup_done)
		return true;
	// ***** Create, setup and insert the content-widget *****:
	if ((csel == ContentSelection::DCsMode) || (csel == ContentSelection::ClearMemoryFcn))
	{
		setContentSelectionButtonChecked(ContentSelection::DCsMode, true);
		_content_DCs = new CUcontent_DCs_engine();
		setContentWidget(tr("Diagnostic Codes:"), _content_DCs);
		_content_DCs->show();
	}
	else if (csel == ContentSelection::MBsSWsMode)
	{
		setContentSelectionButtonChecked(ContentSelection::MBsSWsMode, true);
		_content_MBsSWs = new CUcontent_MBsSWs(_MBSWsettings);
		setContentWidget(tr("Measuring Blocks:"), _content_MBsSWs);
		_content_MBsSWs->show();
	}
	else if (csel == ContentSelection::AdjustmentsMode)
	{
		setContentSelectionButtonChecked(ContentSelection::AdjustmentsMode, true);
		_content_Adjustments = new CUcontent_Adjustments();
		setContentWidget(tr("Adjustments:"), _content_Adjustments);
		_content_Adjustments->show();
	}
	else if (csel == ContentSelection::SysTestsMode)
	{
		setContentSelectionButtonChecked(ContentSelection::SysTestsMode, true);
		_content_SysTests = new CUcontent_sysTests();
		setContentWidget(tr("System Operation Tests:"), _content_SysTests);
		_content_SysTests->show();
	}
	else
		return false;
	// ***** Connect to Control Unit *****:
	// Create Status information message box for CU initialisation/setup:
	FSSM_InitStatusMsgBox initstatusmsgbox(tr("Connecting to Engine Control Unit... Please wait !"), 0, 0, 100, this);
	initstatusmsgbox.setWindowTitle(tr("Connecting..."));
	initstatusmsgbox.setValue(5);
	initstatusmsgbox.show();
	// Try to establish CU connection:
	SSMprotocol::CUsetupResult_dt init_result = probeProtocol(SSMprotocol::CUtype_Engine);
	if ((init_result != SSMprotocol::result_success) && (init_result != SSMprotocol::result_noOrInvalidDefsFile) && (init_result != SSMprotocol::result_noDefs))
		goto commError;
	// Update status info message box:
	initstatusmsgbox.setLabelText(tr("Processing Control Unit data... Please wait !"));
	initstatusmsgbox.setValue(40);
	// Query ROM-ID:
	ROM_ID = _SSMPdev->getROMID();
	if (!ROM_ID.length())
		goto commError;
	// Query system description:
	if (!_SSMPdev->getSystemDescription(&sysdescription))
	{
		SYS_ID = _SSMPdev->getSysID();
		if (!SYS_ID.length())
			goto commError;
		sysdescription = tr("unknown");
		if (SYS_ID != ROM_ID)
			sysdescription += " (" + QString::fromStdString(SYS_ID) + ")";
	}
	// Output system description:
	_infoWidget->setEngineTypeText(sysdescription);
	// Output ROM-ID:
	_infoWidget->setRomIDText( QString::fromStdString(ROM_ID) );
	if (init_result == SSMprotocol::result_success)
	{
		QString VIN = "";
		bool supported = false;
		bool testmode = false;
		bool enginerunning = false;
		std::vector<mb_dt> supportedMBs;
		std::vector<sw_dt> supportedSWs;
		// Number of supported MBs / SWs:
		if ((!_SSMPdev->getSupportedMBs(&supportedMBs)) || (!_SSMPdev->getSupportedSWs(&supportedSWs)))
			goto commError;
		_infoWidget->setNrOfSupportedMBsSWs(supportedMBs.size(), supportedSWs.size());
		// OBD2-Support:
		if (!_SSMPdev->hasOBD2system(&supported))
			goto commError;
		_infoWidget->setOBD2Supported(supported);
		// Integrated Cruise Control:
		if (!_SSMPdev->hasIntegratedCC(&supported))
			goto commError;
		_infoWidget->setIntegratedCCSupported(supported);
		// Immobilizer:
		if (!_SSMPdev->hasImmobilizer(&supported))
			goto commError;
		_infoWidget->setImmobilizerSupported(supported);
		// Update status info message box:
		initstatusmsgbox.setLabelText(tr("Reading Vehicle Ident. Number... Please wait !"));
		initstatusmsgbox.setValue(55);
		// Query and output VIN, if supported:
		if (!_SSMPdev->hasVINsupport(&supported))
			goto commError;
		if (supported)
		{
			if (!_SSMPdev->getVIN(&VIN))
				goto commError;
		}
		_infoWidget->setVINinfo(supported, VIN);
		// Check if we need to stop the automatic actuator test:
		if (!_SSMPdev->hasActuatorTests(&supported))
			goto commError;
		if (supported)
		{
			// Update status info message box:
			initstatusmsgbox.setLabelText(tr("Checking system status... Please wait !"));
			initstatusmsgbox.setValue(70);
			// Query test mode connector status:
			if (!_SSMPdev->isInTestMode(&testmode)) // if actuator tests are available, test mode is available, too...
				goto commError;
			if (testmode)
			{
				// Check that engine is not running:
				if (!_SSMPdev->isEngineRunning(&enginerunning)) // if actuator tests are available, MB "engine speed" is available, too...
					goto commError;
				if (!enginerunning)
				{
					// Update status info message box:
					initstatusmsgbox.setLabelText(tr("Stopping actuators... Please wait !"));
					initstatusmsgbox.setValue(85);
					// Stop all actuator tests:
					if (!_SSMPdev->stopAllActuators())
						goto commError;
				}
			}
		}
		// "Clear Memory"-support:
		if (!_SSMPdev->hasClearMemory(&supported))
			goto commError;
		setContentSelectionButtonEnabled(ContentSelection::ClearMemoryFcn, supported);
		// Enable mode buttons:
		// NOTE: unconditionally, contents are deactivated if unsupported
		setContentSelectionButtonEnabled(ContentSelection::DCsMode, true);
		setContentSelectionButtonEnabled(ContentSelection::MBsSWsMode, true);
		setContentSelectionButtonEnabled(ContentSelection::AdjustmentsMode, true);
		setContentSelectionButtonEnabled(ContentSelection::SysTestsMode, true);
		// Start selected mode:
		bool ok = false;
		if ((csel == ContentSelection::DCsMode) || (csel == ContentSelection::ClearMemoryFcn))
		{
			ok = startDCsMode();
		}
		else if (csel == ContentSelection::MBsSWsMode)
		{
			ok = startMBsSWsMode();
		}
		else if (csel == ContentSelection::AdjustmentsMode)
		{
			ok = startAdjustmentsMode();
		}
		else if (csel == ContentSelection::SysTestsMode)
		{
			ok = startSystemOperationTestsMode();
		}
		// else: BUG
		if (!ok)
			goto commError;
		// Update and close status info:
		initstatusmsgbox.setLabelText(tr("Control Unit initialisation successful !"));
		initstatusmsgbox.setValue(100);
		QTimer::singleShot(800, &initstatusmsgbox, SLOT(accept()));
		initstatusmsgbox.exec();
		initstatusmsgbox.close();
		// Run Clear Memory procedure if requested:
		if (csel == ContentSelection::ClearMemoryFcn)
			clearMemory();
	}
	else
	{
		// Close progress dialog:
		initstatusmsgbox.close();
		// Show error message:
		QString errtext;
		if (init_result == SSMprotocol::result_noOrInvalidDefsFile)
		{
			errtext = tr("Error:\nNo valid definitions file found.\nPlease make sure that FreeSSM is installed properly.");
		}
		else if (init_result == SSMprotocol::result_noDefs)
		{
			errtext = tr("Error:\nThis control unit is not yet supported by FreeSSM.\nFreeSSM can communicate with the control unit, but it doesn't have the necessary data to provide diagnostic operations.\nIf you want to contribute to the the project (help adding defintions), feel free to contact the authors.");
		}
		QMessageBox msg( QMessageBox::Critical, tr("Error"), errtext, QMessageBox::Ok, this);
		QFont msgfont = msg.font();
		msgfont.setPointSize(9);
		msg.setFont( msgfont );
		msg.show();
		msg.exec();
		msg.close();
		// Exit CU dialog:
		close();
		return false;
	}
	_setup_done = true;
	return true;

commError:
	initstatusmsgbox.close();
	communicationError();
	return false;
}


void EngineDialog::switchToDCsMode()
{
	bool ok = false;
	if (_mode == Mode::DCs) return;
	// Show wait-message:
	FSSM_WaitMsgBox waitmsgbox(this, tr("Switching to Diagnostic Codes... Please wait !"));
	waitmsgbox.show();
	// Save content settings:
	saveContentSettings();
	// Create, setup and insert new content-widget:
	_content_DCs = new CUcontent_DCs_engine();
	setContentWidget(tr("Diagnostic Codes:"), _content_DCs);
	_content_DCs->show();
	// Start DCs mode:
	ok = startDCsMode();
	// Close wait-message:
	waitmsgbox.close();
	// Check for communication error:
	if (!ok)
		communicationError();
}


void EngineDialog::switchToMBsSWsMode()
{
	bool ok = false;
	if (_mode == Mode::MBsSWs) return;
	// Show wait-message:
	FSSM_WaitMsgBox waitmsgbox(this, tr("Switching to Measuring Blocks... Please wait !"));
	waitmsgbox.show();
	// Save content settings:
	saveContentSettings();
	// Create, setup and insert new content-widget:
	_content_MBsSWs = new CUcontent_MBsSWs(_MBSWsettings);
	setContentWidget(tr("Measuring Blocks:"), _content_MBsSWs);
	_content_MBsSWs->show();
	// Start MB/SW mode:
	ok = startMBsSWsMode();
	// Close wait-message:
	waitmsgbox.close();
	// Check for communication error:
	if (!ok)
		communicationError();
}


void EngineDialog::switchToAdjustmentsMode()
{
	bool ok = false;
	if (_mode == Mode::Adjustments) return;
	// Show wait-message:
	FSSM_WaitMsgBox waitmsgbox(this, tr("Switching to Adjustment Values... Please wait !"));
	waitmsgbox.show();
	// Save content settings:
	saveContentSettings();
	// Create, setup and insert new content-widget:
	_content_Adjustments = new CUcontent_Adjustments();
	setContentWidget(tr("Adjustments:"), _content_Adjustments);
	_content_Adjustments->show();
	// Start Adjustments mode:
	ok = startAdjustmentsMode();
	// Close wait-message:
	waitmsgbox.close();
	// Check for communication error:
	if (!ok)
		communicationError();
}


void EngineDialog::switchToSystemOperationTestsMode()
{
	bool ok = false;
	if (_mode == Mode::SysTests) return;
	// Show wait-message:
	FSSM_WaitMsgBox waitmsgbox(this, tr("Switching to System Tests... Please wait !"));
	waitmsgbox.show();
	// Save content settings:
	saveContentSettings();
	// Create, setup and insert new content-widget:
	_content_SysTests = new CUcontent_sysTests();
	setContentWidget(tr("System Operation Tests:"), _content_SysTests);
	_content_SysTests->show();
	// Start System Operation Tests mode:
	ok = startSystemOperationTestsMode();
	// Close wait-message:
	waitmsgbox.close();
	// Check for communication error:
	if (!ok)
		communicationError();
}


void EngineDialog::clearMemory()
{
	bool ok = false;
	ClearMemoryDlg::CMresult_dt result;
	// Create "Clear Memory"-dialog:
	ClearMemoryDlg cmdlg(this, _SSMPdev, SSMprotocol::CMlevel_1);
	// Temporary disconnect from "communication error"-signal:
	disconnect(_SSMPdev, SIGNAL( commError() ), this, SLOT( communicationError() ));
	// Run "Clear Memory"-procedure:
	result = cmdlg.run();
	// Reconnect to "communication error"-signal:
	connect(_SSMPdev, SIGNAL( commError() ), this, SLOT( communicationError() ));
	// Check result:
	if ((result == ClearMemoryDlg::CMresult_success) && (_mode == Mode::Adjustments))
	{
		FSSM_WaitMsgBox waitmsgbox(this, tr("Reading Adjustment Values... Please wait !"));
		waitmsgbox.show();
		ok = _content_Adjustments->setup(_SSMPdev); // refresh adjustment values
		waitmsgbox.close();
		if (!ok)
			communicationError();
	}
	else if (result == ClearMemoryDlg::CMresult_communicationError)
	{
		communicationError();
	}
	else if ((result == ClearMemoryDlg::CMresult_reconnectAborted) || (result == ClearMemoryDlg::CMresult_reconnectFailed))
	{
		close(); // exit engine control unit dialog
	}
}


bool EngineDialog::startDCsMode()
{
	int DCgroups = 0;
	if (_content_DCs == NULL)
		return false;
	if (!_content_DCs->setup(_SSMPdev))
		return false;
	if (!_SSMPdev->getSupportedDCgroups(&DCgroups))
		return false;
	if (DCgroups == SSMprotocol::noDCs_DCgroup)
		return false;
	if (!_content_DCs->startDCreading())
		return false;
	connect(_content_DCs, SIGNAL( error() ), this, SLOT( close() ) );
	_mode = Mode::DCs;
	return true;
}


bool EngineDialog::startMBsSWsMode()
{
	if (_content_MBsSWs == NULL)
		return false;
	if (!_content_MBsSWs->setup(_SSMPdev))
		return false;
	if (!_content_MBsSWs->setMBSWselection(_lastMBSWmetaList))
		return false;
	connect(_content_MBsSWs, SIGNAL( error() ), this, SLOT( close() ) );
	_mode = Mode::MBsSWs;
	return true;
}


bool EngineDialog::startAdjustmentsMode()
{
	if (_content_Adjustments == NULL)
		return false;
	if (!_content_Adjustments->setup(_SSMPdev))
		return false;
	connect(_content_Adjustments, SIGNAL( communicationError() ), this, SLOT( close() ) );
	_mode = Mode::Adjustments;
	return true;
}


bool EngineDialog::startSystemOperationTestsMode()
{
	if (_content_SysTests == NULL)
		return false;
	if (!_content_SysTests->setup(_SSMPdev))
		return false;
	connect(_content_SysTests, SIGNAL( error() ), this, SLOT( close() ) );
	_mode = Mode::SysTests;
	return true;
}


void EngineDialog::saveContentSettings()
{
	if (_mode == Mode::MBsSWs)
	{
		_lastMBSWmetaList = _content_MBsSWs->getMBSWselection();
		_MBSWsettings = _content_MBsSWs->getSettings();
	}
}

