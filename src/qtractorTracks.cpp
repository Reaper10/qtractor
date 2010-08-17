// qtractorTracks.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorTracks.h"
#include "qtractorTrackView.h"
#include "qtractorTrackList.h"
#include "qtractorTrackTime.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"

#include "qtractorTrackCommand.h"
#include "qtractorClipCommand.h"
#include "qtractorMidiEditCommand.h"

#include "qtractorTrackButton.h"

#include "qtractorAudioEngine.h"
#include "qtractorAudioBuffer.h"
#include "qtractorAudioClip.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"

#include "qtractorClipSelect.h"

#include "qtractorOptions.h"

#include "qtractorMainForm.h"
#include "qtractorTrackForm.h"
#include "qtractorClipForm.h"

#include "qtractorPasteRepeatForm.h"

#include "qtractorMidiEditorForm.h"

#include <QVBoxLayout>
#include <QProgressBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDate>
#include <QUrl>

#include <QHeaderView>

#if QT_VERSION < 0x040300
#define lighter(x)	light(x)
#define darker(x)	dark(x)
#endif


//----------------------------------------------------------------------------
// qtractorTracks -- The main session track listview widget.

// Constructor.
qtractorTracks::qtractorTracks ( QWidget *pParent )
	: QSplitter(Qt::Horizontal, pParent)
{
	// Surely a name is crucial (e.g. for storing geometry settings)
	QSplitter::setObjectName("qtractorTracks");

	// Create child widgets...
	m_pTrackList = new qtractorTrackList(this, this);
	QWidget *pVBox = new QWidget(this);
	m_pTrackTime = new qtractorTrackTime(this, pVBox);
	m_pTrackView = new qtractorTrackView(this, pVBox);

	// Create child box layouts...
	QVBoxLayout *pVBoxLayout = new QVBoxLayout(pVBox);
	pVBoxLayout->setMargin(0);
	pVBoxLayout->setSpacing(0);
	pVBoxLayout->addWidget(m_pTrackTime);
	pVBoxLayout->addWidget(m_pTrackView);
	pVBox->setLayout(pVBoxLayout);

//	QSplitter::setOpaqueResize(false);
	QSplitter::setStretchFactor(QSplitter::indexOf(m_pTrackList), 0);
	QSplitter::setHandleWidth(2);

	QSplitter::setWindowTitle(tr("Tracks"));
	QSplitter::setWindowIcon(QIcon(":/images/qtractorTracks.png"));

	// Get previously saved splitter sizes,
	// (with some fair default...)
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<int> sizes;
		sizes.append(160);
		sizes.append(480);
		pOptions->loadSplitterSizes(this, sizes);
	}

	// Early track list stabilization.
	m_pTrackTime->setFixedHeight(
		(m_pTrackList->header())->sizeHint().height());

	// To have all views in positional sync.
	QObject::connect(m_pTrackList, SIGNAL(contentsMoving(int,int)),
		m_pTrackView, SLOT(contentsYMovingSlot(int,int)));
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackTime, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackList, SLOT(contentsYMovingSlot(int,int)));
}


// Destructor.
qtractorTracks::~qtractorTracks (void)
{
	// Save splitter sizes...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->saveSplitterSizes(this);
}


// Child widgets accessors.
qtractorTrackList *qtractorTracks::trackList (void) const
{
	return m_pTrackList;
}

qtractorTrackTime *qtractorTracks::trackTime (void) const
{
	return m_pTrackTime;
}

qtractorTrackView *qtractorTracks::trackView (void) const
{
	return m_pTrackView;
}


// Horizontal zoom factor.
void qtractorTracks::horizontalZoomStep ( int iZoomStep )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
		
	int iHorizontalZoom = pSession->horizontalZoom() + iZoomStep;
	if (iHorizontalZoom < ZoomMin)
		iHorizontalZoom = ZoomMin;
	else
	if (iHorizontalZoom > ZoomMax)
		iHorizontalZoom = ZoomMax;
	if (iHorizontalZoom == pSession->horizontalZoom())
		return;

	// Fix the ssession time scale zoom determinant.
	pSession->setHorizontalZoom(iHorizontalZoom);
	pSession->updateTimeScale();
	pSession->updateSessionLength();
}


// Vertical zoom factor.
void qtractorTracks::verticalZoomStep ( int iZoomStep )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
		
	int iVerticalZoom = pSession->verticalZoom() + iZoomStep;
	if (iVerticalZoom < ZoomMin)
		iVerticalZoom = ZoomMin;
	else
	if (iVerticalZoom > ZoomMax)
		iVerticalZoom = ZoomMax;
	if (iVerticalZoom == pSession->verticalZoom())
		return;

	// Fix the session vertical view zoom.
	pSession->setVerticalZoom(iVerticalZoom);
}


// Zoom view actuators.
void qtractorTracks::zoomIn ( int iZoomMode )
{
	if (iZoomMode & ZoomHorizontal)
		horizontalZoomStep(+ ZoomStep);
	if (iZoomMode & ZoomVertical)
		verticalZoomStep(+ ZoomStep);

	centerContents();
}

void qtractorTracks::zoomOut ( int iZoomMode )
{
	if (iZoomMode & ZoomHorizontal)
		horizontalZoomStep(- ZoomStep);
	if (iZoomMode & ZoomVertical)
		verticalZoomStep(- ZoomStep);

	centerContents();
}


void qtractorTracks::zoomReset ( int iZoomMode )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (iZoomMode & ZoomHorizontal)
		horizontalZoomStep(ZoomBase - pSession->horizontalZoom());
	if (iZoomMode & ZoomVertical)
		verticalZoomStep(ZoomBase - pSession->verticalZoom());

	centerContents();
}


void qtractorTracks::horizontalZoomInSlot (void)
{
	horizontalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorTracks::horizontalZoomOutSlot (void)
{
	horizontalZoomStep(- ZoomStep);
	centerContents();
}

void qtractorTracks::verticalZoomInSlot (void)
{
	verticalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorTracks::verticalZoomOutSlot (void)
{
	verticalZoomStep(- ZoomStep);
	centerContents();
}

void qtractorTracks::viewZoomResetSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// All zoom base are belong to us :)
	verticalZoomStep(ZoomBase - pSession->verticalZoom());
	horizontalZoomStep(ZoomBase - pSession->horizontalZoom());
	centerContents();
}


// Try to center horizontally/vertically
// (usually after zoom change)
void qtractorTracks::centerContents (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
		
	// Get current session frame location...
	unsigned long iFrame = m_pTrackView->sessionCursor()->frame();

	// Update the dependant views...
	m_pTrackList->updateContentsHeight();
	m_pTrackView->updateContentsWidth();
	m_pTrackView->setContentsPos(
		pSession->pixelFromFrame(iFrame), m_pTrackView->contentsY());
	m_pTrackView->updateContents();

	// Make its due...
	selectionChangeNotify();
}


// Update/sync integral contents from session tracks.
void qtractorTracks::updateContents ( bool bRefresh )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTracks::updateContents(%d)\n", int(bRefresh));
#endif

	// Update/sync from session tracks.
	int iRefresh = 0;
	if (bRefresh)
		iRefresh++;
	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		// Check if item is already on list
		if (m_pTrackList->trackRow(pTrack) < 0) {
			m_pTrackList->insertTrack(iTrack, pTrack);
			iRefresh++;
		}
		pTrack = pTrack->next();
		iTrack++;
	}

	// Update dependant views.
	if (iRefresh > 0) {
		m_pTrackView->updateContentsWidth();
		m_pTrackList->updateContentsHeight();
	//	m_pTrackView->setFocus();
	}
}


// Retrieves current (selected) track reference.
qtractorTrack *qtractorTracks::currentTrack (void) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	qtractorTrack *pTrack = m_pTrackList->currentTrack();
	if (pTrack == NULL) {
		qtractorClip *pClip = m_pTrackView->currentClip();
		if (pClip) {
			pTrack = pClip->track();
		} else {
			pTrack = pSession->tracks().first();
		}
	}

	return pTrack;
}


// Retrieves current selected clip reference.
qtractorClip *qtractorTracks::currentClip (void) const
{
	return m_pTrackView->currentClip();
}


// Edit/create a brand new clip.
bool qtractorTracks::newClip (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Create on current track, or take the first...
	qtractorTrack *pTrack = currentTrack();
	if (pTrack == NULL)
		pTrack = pSession->tracks().first();
	if (pTrack == NULL)
		return false;

	// Create the clip prototype...
	qtractorClip *pClip = NULL;
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
		pClip = new qtractorAudioClip(pTrack);
		break;
	case qtractorTrack::Midi:
		pClip = new qtractorMidiClip(pTrack);
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Correct so far?
	if (pClip == NULL)
		return false;

	// Set initial default clip parameters...
	unsigned long iClipStart = pSession->editHead();
	pClip->setClipStart(iClipStart);
	m_pTrackView->ensureVisibleFrame(iClipStart);

	// Special for MIDI clips, which already have it's own editor,
	// we'll add and start a blank one right-away...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Set initial clip length...
		if (pSession->editTail() > pSession->editHead()) {
			pClip->setClipLength(pSession->editTail() - pSession->editHead());
		} else {
			pClip->setClipLength(pSession->frameFromTick(
				pSession->ticksPerBeat() * pSession->beatsPerBar()));
		}
		// Create a clip filename from scratch...
		const QString& sFilename = pSession->createFilePath(
			pTrack->trackName(), pTrack->clips().count(), "mid");
		pClip->setFilename(sFilename);
		pClip->setClipName(QFileInfo(sFilename).baseName());
		// Proceed to setup the MDII clip properly...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			// Initialize MIDI event container...
			qtractorMidiSequence *pSeq = pMidiClip->sequence();
			pSeq->setName(pMidiClip->clipName());
			pSeq->setChannel(pTrack->midiChannel());
			pSeq->setTicksPerBeat(pSession->ticksPerBeat());
			// Which SMF format?
			if (pMidiClip->format() == 0) {
				// SMF format 0 (1 track, 1 channel)
				pMidiClip->setTrackChannel(pTrack->midiChannel());
			} else {
				// SMF format 1 (2 tracks, 1 channel)
				pMidiClip->setTrackChannel(1);
			}
			// Make it a brand new revision...
			pMidiClip->setRevision(1);
			// Insert the clip right away...
			qtractorClipCommand *pClipCommand
				= new qtractorClipCommand(tr("new clip"));
			pClipCommand->addClip(pClip, pTrack);
			pSession->execute(pClipCommand);
			// Just start the MIDI editor on it...
			return pClip->startEditor(pMainForm);
		}
	}

	// Then ask user to refine clip properties...
	qtractorClipForm clipForm(pMainForm);
	clipForm.setClip(pClip, true);
	if (!clipForm.exec()) {
		delete pClip;
		return false;
	}

	// Done.
	return true;
}


// Edit given(current) clip.
bool qtractorTracks::editClip ( qtractorClip *pClip )
{
	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// All else hasn't fail.
	return pClip->startEditor(pMainForm);
}


// Split given(current) clip.
bool qtractorTracks::splitClip ( qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	unsigned long iPlayHead  = pSession->playHead();
	unsigned long iClipStart = pClip->clipStart();
	unsigned long iClipEnd   = iClipStart + pClip->clipLength();
	if (iClipStart >= iPlayHead || iPlayHead >= iClipEnd)
		return false;

	m_pTrackView->ensureVisibleFrame(iPlayHead);

	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("split clip"));

	// Shorten old right...
	unsigned long iClipOffset = pClip->clipOffset();
	pClipCommand->moveClip(pClip, pClip->track(),
		iClipStart, iClipOffset, iPlayHead - iClipStart);
	// Add left clone...
	qtractorClip *pNewClip = m_pTrackView->cloneClip(pClip);
	if (pNewClip) {
		pNewClip->setClipStart(iPlayHead);
		pNewClip->setClipOffset(iClipOffset + iPlayHead - iClipStart);
		pNewClip->setClipLength(iClipEnd - iPlayHead);
		pNewClip->setFadeOutLength(pClip->fadeOutLength());
		pClipCommand->addClip(pNewClip, pNewClip->track());
	}

	// That's it...
	return pSession->execute(pClipCommand);
}


// Audio clip normalize callback.
struct audioClipNormalizeData
{	// Ctor.
	audioClipNormalizeData(unsigned short iChannels)
		: count(0), channels(iChannels), max(0.0f) {};
	// Members.
	unsigned int count;
	unsigned short channels;
	float max;
};

static void audioClipNormalize (
	float **ppFrames, unsigned int iFrames, void *pvArg )
{
	audioClipNormalizeData *pData
		= static_cast<audioClipNormalizeData *> (pvArg);

	for (unsigned short i = 0; i < pData->channels; ++i) {
		float *pFrames = ppFrames[i];
		for (unsigned int n = 0; n < iFrames; ++n) {
			float fSample = *pFrames++;
			if (fSample < 0.0f) // Take absolute value...
				fSample = -(fSample);
			if (pData->max < fSample)
				pData->max = fSample;
		}
	}

	if (++(pData->count) > 100) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			QProgressBar *pProgressBar = pMainForm->progressBar();
			pProgressBar->setValue(pProgressBar->value() + iFrames);
		}
		qtractorSession::stabilize();
		pData->count = 0;
	}
}


// MIDI clip normalize callback.
static void midiClipNormalize (
	qtractorMidiSequence *pSeq, void *pvArg )
{
	unsigned char *pMax = (unsigned char *) (pvArg);
	for (qtractorMidiEvent *pEvent = pSeq->events().first();
			pEvent; pEvent = pEvent->next()) {
		if (pEvent->type() == qtractorMidiEvent::NOTEON &&
			*pMax < pEvent->velocity()) {
			*pMax = pEvent->velocity();
		}
	}
}


// Normalize given(current) clip.
bool qtractorTracks::normalizeClip ( qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Make it as an undoable command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("clip normalize"));

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Multiple clip selection...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	if (pClipSelect->items().count() > 0) {
		QListIterator<qtractorClipSelect::Item *> iter(pClipSelect->items());
		while (iter.hasNext())
			normalizeClipCommand(pClipCommand, iter.next()->clip);
	}	// Single, current clip instead?
	else normalizeClipCommand(pClipCommand, pClip);

	QApplication::restoreOverrideCursor();

	// Check if valid...
	if (pClipCommand->isEmpty()) {
		delete pClipCommand;
		return false;
	}

	// That's it...
	return pSession->execute(pClipCommand);

}


bool qtractorTracks::normalizeClipCommand (
	qtractorClipCommand *pClipCommand, qtractorClip *pClip )
{
	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();

	unsigned long iOffset = 0;
	unsigned long iLength = pClip->clipLength();

	if (pClip->isClipSelected()) {
		iOffset = pClip->clipSelectStart() - pClip->clipStart();
		iLength = pClip->clipSelectEnd() - pClip->clipSelectStart();
	}

	// Default non-normalized setting...
	float fGain = pClip->clipGain();

	if (pTrack->trackType() == qtractorTrack::Audio) {
		// Normalize audio clip...
		qtractorAudioClip *pAudioClip
			= static_cast<qtractorAudioClip *> (pClip);
		if (pAudioClip == NULL)
			return false;
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pTrack->outputBus());
		if (pAudioBus == NULL)
			return false;
		QProgressBar *pProgressBar = NULL;
		if (pMainForm)
			pProgressBar = pMainForm->progressBar();
		if (pProgressBar) {
			pProgressBar->setRange(0, iLength / 100);
			pProgressBar->reset();
			pProgressBar->show();
		}
		audioClipNormalizeData data(pAudioBus->channels());
		pAudioClip->clipExport(audioClipNormalize, &data, iOffset, iLength);
		if (data.max > 0.01f && data.max < 1.1f)
			fGain /= data.max;
		if (pProgressBar)
			pProgressBar->hide();
	}
	else
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Normalize MIDI clip...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip == NULL)
			return false;
		unsigned char max = 0;
		pMidiClip->clipExport(midiClipNormalize, &max, iOffset, iLength);
		if (max > 0x0c && max < 0x7f)
			fGain *= (127.0f / float(max));
	}

	// Make it as an undoable command...
	pClipCommand->gainClip(pClip, fGain);

	// That's it...
	return true;
}


// Quantize given(current) MIDI clip.
bool qtractorTracks::quantizeClip ( qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Make it as an undoable command...
	qtractorMidiClipCommand *pMidiClipCommand
		= new qtractorMidiClipCommand(tr("clip quantize"));

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Multiple clip selection...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	if (pClipSelect->items().count() > 0) {
		QListIterator<qtractorClipSelect::Item *> iter(pClipSelect->items());
		while (iter.hasNext())
			quantizeClipCommand(pMidiClipCommand, iter.next()->clip);
	}	// Single, current clip instead?
	else quantizeClipCommand(pMidiClipCommand, pClip);

	QApplication::restoreOverrideCursor();

	// Check if valid...
	if (pMidiClipCommand->isEmpty()) {
		delete pMidiClipCommand;
		return false;
	}

	// That's it...
	return pSession->execute(pMidiClipCommand);
}


bool qtractorTracks::quantizeClipCommand (
	qtractorMidiClipCommand *pMidiClipCommand, qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	unsigned short iQuantize = pSession->snapPerBeat();
	if (iQuantize < 1)
		return false;

	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == NULL)
		return false;
	if (pTrack->trackType() != qtractorTrack::Midi)
		return false;

	qtractorMidiClip *pMidiClip = static_cast<qtractorMidiClip *> (pClip);
	if (pMidiClip == NULL)
		return false;

	unsigned long iOffset = 0;
	unsigned long iLength = pClip->clipLength();

	if (pClip->isClipSelected()) {
		iOffset = pClip->clipSelectStart() - pClip->clipStart();
		iLength = pClip->clipSelectEnd() - pClip->clipSelectStart();
	}

	// Make it as an undoable command...
	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(pMidiClip, tr("clip quantize"));

	qtractorMidiSequence *pSeq = pMidiClip->sequence();
	unsigned long iTimeOffset = pSeq->timeOffset();

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pClip->clipStart());
	unsigned long t0 = pNode->tickFromFrame(pClip->clipStart());

	unsigned long f1 = pClip->clipStart() + pClip->clipOffset() + iOffset;
	pNode = cursor.seekFrame(f1);
	unsigned long t1 = pNode->tickFromFrame(f1);
	unsigned long iTimeStart = t1 - t0;
	iTimeStart = (iTimeStart > iTimeOffset ? iTimeStart - iTimeOffset : 0);
	pNode = cursor.seekFrame(f1 += iLength);
	unsigned long iTimeEnd = iTimeStart + pNode->tickFromFrame(f1) - t1;

	for (qtractorMidiEvent *pEvent = pSeq->events().first();
			pEvent; pEvent = pEvent->next()) {
		unsigned long iTime = pEvent->time();
		if (iTime >= iTimeStart && iTime < iTimeEnd) {
			pNode = cursor.seekTick(iTime);
			unsigned long q = pNode->ticksPerBeat / iQuantize;
			iTime = q * ((iTime + (q >> 1)) / q);
			unsigned long iDuration = pEvent->duration();
			if (pEvent->type() == qtractorMidiEvent::NOTEON)
				iDuration = q * ((iDuration + q - 1) / q);
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
	}

	// Must be brand new revision...
	pMidiClip->setRevision(0);

	// That's it...
	pMidiClipCommand->addEditCommand(pEditCommand);
	return true;
}


// Import (audio) clip(s) into current track...
bool qtractorTracks::importClips (
	QStringList files, unsigned long iClipStart )
{
	// Have we some?
	if (files.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Create on current track, or take the first...
	qtractorTrack *pTrack = currentTrack();
	if (pTrack == NULL)
		pTrack = pSession->tracks().first();
	if (pTrack == NULL) // || pTrack->trackType() != qtractorTrack::Audio)
		return addAudioTracks(files, iClipStart);

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	qtractorClipCommand *pImportClipCommand
		= new qtractorClipCommand(tr("clip import"));

	// For each one of those files...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// This is one of the selected filenames....
		const QString& sPath = iter.next();
		switch (pTrack->trackType()) {
		case qtractorTrack::Audio: {
			// Add the audio clip at once...
			qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
			pAudioClip->setFilename(sPath);
			pAudioClip->setClipStart(iClipStart);
			// Redundant but necessary for multi-clip
			// concatenation, as we only know the actual
			// audio clip length after opening it...
			if (iter.hasNext()) {
				pAudioClip->open();
				iClipStart += pAudioClip->clipLength();
			}
			// Will add the new clip into session on command execute...
			pImportClipCommand->addClip(pAudioClip, pTrack);
			// Don't forget to add this one to local repository.
			if (pMainForm) {
				pMainForm->addAudioFile(sPath);
				// Log this successful import operation...
				sDescription += tr("Audio file import \"%1\" on %2 %3.\n")
					.arg(QFileInfo(sPath).fileName())
					.arg(QDate::currentDate().toString("MMM dd yyyy"))
					.arg(QTime::currentTime().toString("hh:mm:ss"));
				pMainForm->appendMessages(
					tr("Audio file import: \"%1\".").arg(sPath));
			}
			break;
		}
		case qtractorTrack::Midi: {
			// We'll be careful and pre-open the SMF header here...
			qtractorMidiFile file;
			if (file.open(sPath)) {
				// Depending of SMF format...
				int iTrackChannel = pTrack->midiChannel();
				if (file.format() == 1)
					iTrackChannel++;
				// Add the MIDI clip at once...
				qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
				pMidiClip->setFilename(sPath);
				pMidiClip->setTrackChannel(iTrackChannel);
				pMidiClip->setClipStart(iClipStart);
				// Redundant but necessary for multi-clip
				// concatenation, as we only know the actual
				// MIDI clip length after opening it...
				if (iter.hasNext()) {
					pMidiClip->open();
					iClipStart += pMidiClip->clipLength();
				}
				// Will add the new clip into session on command execute...
				pImportClipCommand->addClip(pMidiClip, pTrack);
				// Don't forget to add this one to local repository.
				if (pMainForm) {
					pMainForm->addMidiFile(sPath);
					// Log this successful import operation...
					sDescription += tr("MIDI file import \"%1\""
						" track-channel %2 on %3 %4.\n")
						.arg(QFileInfo(sPath).fileName()).arg(iTrackChannel)
						.arg(QDate::currentDate().toString("MMM dd yyyy"))
						.arg(QTime::currentTime().toString("hh:mm:ss"));
					pMainForm->appendMessages(
						tr("MIDI file import: \"%1\", track-channel: %2.")
						.arg(sPath).arg(iTrackChannel));
				}
			}
			break;
		}
		default:
			break;
		}
	}

	// Log to session (undoable by import-track command)...
	pSession->setDescription(sDescription);

	// Done.
	return pSession->execute(pImportClipCommand);
}


// Export selected clips.
bool qtractorTracks::exportClips (void)
{
	return mergeExportClips(NULL);
}


// Merge selected clips.
bool qtractorTracks::mergeClips (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Make it as an undoable command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("clip merge"));

	bool bResult = mergeExportClips(pClipCommand);
	
	// Have we failed the command prospect?
	if (!bResult || pClipCommand->isEmpty()) {
		delete pClipCommand;
		return false;
	}

	return pSession->execute(pClipCommand);
}


// Merge/export selected clips.
bool qtractorTracks::mergeExportClips ( qtractorClipCommand *pClipCommand )
{
	// Multiple clip selection:
	// - make sure we have at least 2 clips to merge...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	if (pClipSelect->items().count() < 1)
		return false;

	// Should be one single track...
	qtractorTrack *pTrack = pClipSelect->singleTrack();
	if (pTrack == NULL)
		return false;

	// Dispatch to specialized method...
	bool bResult = false;
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
		bResult = mergeExportAudioClips(pClipCommand);
		break;
	case qtractorTrack::Midi:
		bResult = mergeExportMidiClips(pClipCommand);
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Done most.
	return bResult;
}


// Audio clip buffer merge/export item.
struct audioClipBufferItem
{
	// Constructor.
	audioClipBufferItem(qtractorClip *pClip,
		unsigned short iChannels,
		unsigned int iSampleRate)
		: clip(static_cast<qtractorAudioClip *> (pClip))
	{
		buff = new qtractorAudioBuffer(iChannels, iSampleRate);
		buff->setOffset(clip->clipOffset());
		buff->setLength(clip->clipLength());
		buff->setTimeStretch(clip->timeStretch());
		buff->setPitchShift(clip->pitchShift());
		buff->open(clip->filename());
	}

	// Destructor.
	~audioClipBufferItem()
		{ if (buff) delete buff; }

	// Members.
	qtractorAudioClip   *clip;
	qtractorAudioBuffer *buff;
};


// Merge/export selected(audio) clips.
bool qtractorTracks::mergeExportAudioClips ( qtractorClipCommand *pClipCommand )
{
	// Should be one single MIDI track...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	qtractorTrack *pTrack = pClipSelect->singleTrack();
	if (pTrack == NULL)
		return false;
	if (pTrack->trackType() != qtractorTrack::Audio)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (pTrack->outputBus());
	if (pAudioBus == NULL)
		return false;

	const QString& sExt = qtractorAudioFileFactory::defaultExt();
	const QString& sTitle  = tr("Merge/Export Audio Clip") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Audio files (*.%1)").arg(sExt); 
#if QT_VERSION < 0x040400
	// Ask for the filename to save...
	QString sFilename = QFileDialog::getSaveFileName(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		urls.append(QUrl::fromLocalFile(pOptions->sAudioDir));
		fileDialog.setSidebarUrls(urls);
	}
	// Show dialog...
	if (!fileDialog.exec())
		return false;
	QString sFilename = fileDialog.selectedFiles().first();
#endif
	if (sFilename.isEmpty())
		return false;
	if (QFileInfo(sFilename).suffix().isEmpty())
		sFilename += '.' + sExt;

	// Should take sometime...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	qtractorAudioFile *pAudioFile
		= qtractorAudioFileFactory::createAudioFile(sFilename,
			pAudioBus->channels(), pSession->sampleRate());
	if (pAudioFile == NULL) {
		QApplication::restoreOverrideCursor();
		return false;
	}

	// Open the file for writing...
	if (!pAudioFile->open(sFilename, qtractorAudioFile::Write)) {
		QApplication::restoreOverrideCursor();
		delete pAudioFile;
		return false;
	}

	// Start logging...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		pMainForm->appendMessages(
			tr("Audio clip merge/export: \"%1\" started...")
			.arg(sFilename));
	}

	// Multiple clip selection...
	QListIterator<qtractorClipSelect::Item *> iter(pClipSelect->items());

	// Multi-selection extents (in frames)...
	QList<audioClipBufferItem *> items;
	unsigned long iSelectStart = pSession->sessionLength();
	unsigned long iSelectEnd = 0;
	while (iter.hasNext()) {
		qtractorClip *pClip = iter.next()->clip;
		if (iSelectStart > pClip->clipSelectStart())
			iSelectStart = pClip->clipSelectStart();
		if (iSelectEnd < pClip->clipSelectEnd())
			iSelectEnd = pClip->clipSelectEnd();
		items.append(new audioClipBufferItem(
			pClip, pAudioBus->channels(), pSession->sampleRate()));
	}

	// A progress indication might be friendly...
	QProgressBar *pProgressBar = NULL;
	if (pMainForm)
		pProgressBar = pMainForm->progressBar();
	if (pProgressBar) {
		pProgressBar->setRange(0, (iSelectEnd - iSelectStart) / 100);
		pProgressBar->reset();
		pProgressBar->show();
	}

	// Allocate merge audio scratch buffer...
	unsigned int iBufferSize = pSession->audioEngine()->bufferSize();
	unsigned short iChannels = pAudioBus-> channels();
	unsigned short i;
	float **ppFrames = new float * [iChannels];
	for (i = 0; i < iChannels; ++i)
		ppFrames[i] = new float[iBufferSize];

	// Setup clip buffers...
	QListIterator<audioClipBufferItem *> it(items);
	while (it.hasNext()) {
		audioClipBufferItem *pItem = it.next();
		qtractorAudioClip   *pClip = pItem->clip;
		qtractorAudioBuffer *pBuff = pItem->buff;
		// Almost similar to qtractorAudioClip::process(0)...
		unsigned long iOffset = 0;
		unsigned long iClipStart = pClip->clipStart();
		unsigned long iClipEnd   = iClipStart + pClip->clipLength();
		if (iSelectStart > iClipStart && iSelectStart < iClipEnd)
			iOffset = iSelectStart - iClipStart;
		// Make it initially filled...
		pBuff->seek(iOffset);
	//	pBuff->syncExport();
	}

	// Loop-merge audio clips...
	unsigned long iFrameStart = iSelectStart;
	unsigned long iFrameEnd = iFrameStart + iBufferSize;
	int count = 0;

	// Loop until EOF...
	while (iFrameStart < iSelectEnd && iFrameEnd > iSelectStart) {
		// Zero-silence on scratch buffers...
		for (i = 0; i < iChannels; ++i)
			::memset(ppFrames[i], 0, iBufferSize * sizeof(float));
		// Merge clips in window...
		it.toFront();
		while (it.hasNext()) {
			audioClipBufferItem *pItem = it.next();
			qtractorAudioClip   *pClip = pItem->clip;
			qtractorAudioBuffer *pBuff = pItem->buff;
			// Should force sync now and then...
			if ((count % 33) == 0) pBuff->syncExport();
			// Quite similar to qtractorAudioClip::process()...
			unsigned long iClipStart = pClip->clipStart();
			unsigned long iClipEnd   = iClipStart + pClip->clipLength();
			if (iFrameStart < iClipStart && iFrameEnd > iClipStart) {
				unsigned long iOffset = iFrameEnd - iClipStart;
				if (pBuff->inSync(0, iOffset)) {
					pBuff->readMix(ppFrames, iOffset,
						iChannels, iClipStart - iFrameStart,
						pClip->gain(iOffset));
				}
			}
			else
			if (iFrameStart >= iClipStart && iFrameStart < iClipEnd) {
				unsigned long iOffset = iFrameEnd - iClipStart;
				if (pBuff->inSync(iFrameStart - iClipStart, iOffset)) {
					pBuff->readMix(ppFrames, iFrameEnd - iFrameStart,
						iChannels, 0, pClip->gain(iOffset));
				}
			}
		}
		// Actually write to merge audio file;
		// - check for last incomplete block...
		if (iFrameEnd > iSelectEnd)
			pAudioFile->write(ppFrames, iBufferSize - (iFrameEnd - iSelectEnd));
		else
			pAudioFile->write(ppFrames, iBufferSize);
		// Advance to next buffer...
		iFrameStart = iFrameEnd;
		iFrameEnd = iFrameStart + iBufferSize;
		if (++count > 100 && pProgressBar) {
			pProgressBar->setValue(pProgressBar->value() + iBufferSize);
			qtractorSession::stabilize();
			count = 0;
		}
	}

	for (i = 0; i < iChannels; ++i)
		delete ppFrames[i];
	delete [] ppFrames;

	qDeleteAll(items);
	items.clear();

	// Close and free it up...
	pAudioFile->close();
	delete pAudioFile;

	if (pProgressBar)
		pProgressBar->hide();

	// Stop logging...
	if (pMainForm) {
		pMainForm->addAudioFile(sFilename);
		pMainForm->appendMessages(
			tr("Audio clip merge/export: \"%1\" complete.")
			.arg(sFilename));
	}

	// The resulting merge comands, if any...
	if (pClipCommand) {
		iter.toFront();
		while (iter.hasNext()) {
			qtractorClip *pClip = iter.next()->clip;
			// Clip parameters.
			unsigned long iClipStart  = pClip->clipStart();
			unsigned long iClipOffset = pClip->clipOffset();
			unsigned long iClipLength = pClip->clipLength();
			unsigned long iClipEnd    = iClipStart + iClipLength;
			// Determine and keep clip regions...
			if (iSelectStart > iClipStart) {
				// -- Left clip...
				pClipCommand->resizeClip(pClip,
					iClipStart,
					iClipOffset,
					iSelectStart - iClipStart);
				// Done, left clip.
			}
			else
			if (iSelectEnd < iClipEnd) {
				// -- Right clip...
				pClipCommand->resizeClip(pClip,
					iSelectEnd,
					iClipOffset + (iSelectEnd - iClipStart),
					iClipEnd - iSelectEnd);
				// Done, right clip.
			} else {
				// -- Inner clip...
				pClipCommand->removeClip(pClip);
				// Done, inner clip.
			}
		}
		// Set the resulting clip command...
		qtractorAudioClip *pNewClip = new qtractorAudioClip(pTrack);
		pNewClip->setClipStart(iSelectStart);
		pNewClip->setClipLength(iSelectEnd - iSelectStart);
		pNewClip->setFilename(sFilename);
		pClipCommand->addClip(pNewClip, pTrack);
	}

	// Almost done with it...
	QApplication::restoreOverrideCursor();

	// That's it...
	return true;
}


// Merge/export selected(MIDI) clips.
bool qtractorTracks::mergeExportMidiClips ( qtractorClipCommand *pClipCommand )
{
	// Should be one single MIDI track...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	qtractorTrack *pTrack = pClipSelect->singleTrack();
	if (pTrack == NULL)
		return false;
	if (pTrack->trackType() != qtractorTrack::Midi)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Merge MIDI Clip filename requester...
	const QString  sExt("mid");
	const QString& sTitle  = tr("Merge/Export MIDI Clip") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("MIDI files (*.%1 *.smf *.midi)").arg(sExt); 
#if QT_VERSION < 0x040400
	// Ask for the filename to save...
	QString sFilename = QFileDialog::getSaveFileName(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		urls.append(QUrl::fromLocalFile(pOptions->sMidiDir));
		fileDialog.setSidebarUrls(urls);
	}
	// Show dialog...
	if (!fileDialog.exec())
		return false;
	QString sFilename = fileDialog.selectedFiles().first();
#endif
	if (sFilename.isEmpty())
		return false;
	if (QFileInfo(sFilename).suffix().isEmpty())
		sFilename += '.' + sExt;

	// Should take sometime...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Create SMF...
	qtractorMidiFile file;
	if (!file.open(sFilename, qtractorMidiFile::Write)) {
		QApplication::restoreOverrideCursor();
		return false;
	}

	// Write SMF header...
	unsigned short iTicksPerBeat = pSession->ticksPerBeat();
	unsigned short iFormat = qtractorMidiClip::defaultFormat();
	unsigned short iTracks = (iFormat == 0 ? 1 : 2);
	if (!file.writeHeader(iFormat, iTracks, iTicksPerBeat)) {
		QApplication::restoreOverrideCursor();
		return false;
	}

	// Start logging...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		pMainForm->appendMessages(
			tr("MIDI clip merge/export: \"%1\" started...")
			.arg(sFilename));
	}

	// Multiple clip selection...
	QListIterator<qtractorClipSelect::Item *> iter(pClipSelect->items());

	// Multi-selection extents (in frames)...
	unsigned long iSelectStart = pSession->sessionLength();
	unsigned long iSelectEnd = 0;
	while (iter.hasNext()) {
		qtractorClip *pClip = iter.next()->clip;
		if (iSelectStart > pClip->clipSelectStart())
			iSelectStart = pClip->clipSelectStart();
		if (iSelectEnd < pClip->clipSelectEnd())
			iSelectEnd = pClip->clipSelectEnd();
	}

	// Multi-selection extents (in ticks)...
	unsigned long iTimeStart = pSession->tickFromFrame(iSelectStart);
	unsigned long iTimeEnd   = pSession->tickFromFrame(iSelectEnd);

	// Set proper tempo map...
	if (file.tempoMap()) {
		file.tempoMap()->fromTimeScale(
			pSession->timeScale(),
			pSession->tickFromFrame(iTimeStart));
	}

	// Setup track (SMF format 1).
	if (iFormat == 1)
		file.writeTrack(NULL);

	// Setup merge sequence...
	qtractorMidiSequence seq(pTrack->trackName(), 1, iTicksPerBeat);
	seq.setChannel(pTrack->midiChannel());
	seq.setBank(pTrack->midiBank());
	seq.setProgram(pTrack->midiProgram());

	// The merge...
	iter.toFront();
	while (iter.hasNext()) {
		qtractorClip *pClip = iter.next()->clip;
		// Clip parameters.
		unsigned long iClipStart  = pClip->clipStart();
		unsigned long iClipOffset = pClip->clipOffset();
		unsigned long iClipLength = pClip->clipLength();
		unsigned long iClipEnd    = iClipStart + iClipLength;
		// Determine and keep clip regions...
		if (pClipCommand) {
			if (iSelectStart > iClipStart) {
				// -- Left clip...
				pClipCommand->resizeClip(pClip,
					iClipStart,
					iClipOffset,
					iSelectStart - iClipStart);
				// Done, left clip.
			}
			else
			if (iSelectEnd < iClipEnd) {
				// -- Right clip...
				pClipCommand->resizeClip(pClip,
					iSelectEnd,
					iClipOffset + (iSelectEnd - iClipStart),
					iClipEnd - iSelectEnd);
				// Done, right clip.
			} else {
				// -- Inner clip...
				pClipCommand->removeClip(pClip);
				// Done, inner clip.
			}
		}
		// Do the MIDI merge, itself...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			unsigned long iTimeClip
				= pSession->tickFromFrame(pClip->clipStart());
			unsigned long iTimeOffset = iTimeClip - iTimeStart;
			// For each event...
			qtractorMidiEvent *pEvent
				= pMidiClip->sequence()->events().first();
			while (pEvent && iTimeClip + pEvent->time() < iTimeStart)
				pEvent = pEvent->next();
			while (pEvent && iTimeClip + pEvent->time() < iTimeEnd) {
				qtractorMidiEvent *pNewEvent
					= new qtractorMidiEvent(*pEvent);
				pNewEvent->setTime(iTimeOffset + pEvent->time());
				if (pNewEvent->type() == qtractorMidiEvent::NOTEON) {
					unsigned long iTimeEvent = iTimeClip + pEvent->time();
					float fGain = pMidiClip->gain(
						pSession->frameFromTick(iTimeEvent)
						- pClip->clipStart());
					pNewEvent->setVelocity((unsigned char)
						(fGain * float(pEvent->velocity())) & 0x7f);
					if (iTimeEvent + pEvent->duration() > iTimeEnd)
						pNewEvent->setDuration(iTimeEnd - iTimeEvent);
				}
				seq.insertEvent(pNewEvent);
				pEvent = pEvent->next();
			}
		}
	}

	// Write the track and close SMF...
	file.writeTrack(&seq);
	file.close();

	// Stop logging...
	if (pMainForm) {
		pMainForm->addMidiFile(sFilename);
		pMainForm->appendMessages(
			tr("MIDI clip merge/export: \"%1\" complete.")
			.arg(sFilename));
	}

	// Set the resulting clip command...
	if (pClipCommand) {
		qtractorMidiClip *pNewClip = new qtractorMidiClip(pTrack);
		pNewClip->setClipStart(iSelectStart);
		pNewClip->setClipLength(iSelectEnd - iSelectStart);
		pNewClip->setFilename(sFilename);
		pNewClip->setTrackChannel(1);
		pClipCommand->addClip(pNewClip, pTrack);
	}

	// Almost done with it...
	QApplication::restoreOverrideCursor();

	// That's it...
	return true;
}


// Whether there's any clip currently selected.
bool qtractorTracks::isClipSelected (void) const
{
	return m_pTrackView->isClipSelected();
}


// Whether there's a single track selection.
qtractorTrack *qtractorTracks::singleTrackSelected (void)
{
	return m_pTrackView->singleTrackSelected();
}


// Clipboard methods.
void qtractorTracks::cutClipboard (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Cut);
}

void qtractorTracks::copyClipboard (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Copy);
}

void qtractorTracks::pasteClipboard (void)
{
	m_pTrackView->pasteClipboard();
}


// Special paste/repeat prompt.
void qtractorTracks::pasteRepeatClipboard (void)
{
	qtractorPasteRepeatForm pasteForm(this);
	pasteForm.setRepeatPeriod(m_pTrackView->pastePeriod());
	if (pasteForm.exec()) {
		m_pTrackView->pasteClipboard(
			pasteForm.repeatCount(),
			pasteForm.repeatPeriod());
	}
}



// Delete current selection.
void qtractorTracks::deleteClipSelect (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Delete);
}


// Select range interval between edit head and tail.
void qtractorTracks::selectEditRange (void)
{
	m_pTrackView->selectEditRange();
}



// Select all clips on current track.
void qtractorTracks::selectCurrentTrack ( bool bReset )
{
	qtractorTrack *pTrack = currentTrack();
	if (pTrack)
		m_pTrackView->selectTrack(pTrack, bReset);
}


// Select all clips on all tracks.
void qtractorTracks::selectAll ( bool bSelect )
{
	m_pTrackView->selectAll(bSelect);
}


// Adds a new track into session.
bool qtractorTracks::addTrack (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Create a new track right away...
	const int iTrack = pSession->tracks().count() + 1;
	const QColor color = qtractorTrack::trackColor(iTrack);
	qtractorTrack *pTrack = new qtractorTrack(pSession);
	pTrack->setTrackName(QString("Track %1").arg(iTrack));
	pTrack->setMidiChannel(pSession->midiTag() % 16);
	pTrack->setBackground(color);
	pTrack->setForeground(color.darker());

	// Open dialog for settings...
	qtractorTrackForm trackForm(pMainForm);
	trackForm.setTrack(pTrack);
	if (!trackForm.exec()) {
		delete pTrack;
		return false;
	}

	// Take care of user supplied properties...
	pTrack->properties() = trackForm.properties();

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorAddTrackCommand(pTrack));
}


// Remove given(current) track from session.
bool qtractorTracks::removeTrack ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Get the list view item reference of the intended track...
	if (pTrack == NULL)
		pTrack = currentTrack();
	if (pTrack == NULL)
		return false;

	// Don't remove tracks engaged in recording...
	if (pTrack->isRecord() && pSession->isRecording() && pSession->isPlaying())
		return false;

	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to remove track:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(pTrack->trackName()),
			QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Cancel)
			return false;
	}

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorRemoveTrackCommand(pTrack));
}


// Edit given(current) track properties.
bool qtractorTracks::editTrack ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Get the list view item reference of the intended track...
	if (pTrack == NULL)
		pTrack = currentTrack();
	if (pTrack == NULL)
		return false;

	// Don't edit tracks engaged in recording...
	if (pTrack->isRecord() && pSession->isRecording() && pSession->isPlaying())
		return false;

	// Open dialog for settings...
	qtractorTrackForm trackForm(pMainForm);
	trackForm.setTrack(pTrack);
	if (!trackForm.exec())
		return false;

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorEditTrackCommand(pTrack, trackForm.properties()));
}


// Import Audio files into new tracks...
bool qtractorTracks::addAudioTracks (
	QStringList files, unsigned long iClipStart )
{
	// Have we some?
	if (files.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	pSession->lock();

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand();

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// Needed whether we'll span to one single track
	// or will have each clip intto several tracks...
	bool bDropSpan = m_pTrackView->isDropSpan();
	qtractorTrack *pTrack = NULL;
	int iTrackClip = 0;

	// For each one of those files...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// This is one of the selected filenames....
		const QString& sPath = iter.next();
		// Create a new track right away...
		if (pTrack == NULL || !bDropSpan) {
			const QColor& color = qtractorTrack::trackColor(++iTrack);
			pTrack = new qtractorTrack(pSession, qtractorTrack::Audio);
			pTrack->setBackground(color);
			pTrack->setForeground(color.darker());
			pImportTrackCommand->addTrack(pTrack);
			iTrackClip = 0;
		}
		// Add the clip at once...
		qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
		pAudioClip->setFilename(sPath);
		pAudioClip->setClipStart(iClipStart);
		// Time to add the new track/clip into session;
		// actuallly, this is when the given audio file gets open...
		pTrack->addClip(pAudioClip);
		if (iTrackClip == 0)
			pTrack->setTrackName(pAudioClip->clipName());
		iTrackClip++;
		// Add the new track to composite command...
		if (bDropSpan)
			iClipStart += pAudioClip->clipLength();
		iUpdate++;
		// Don't forget to add this one to local repository.
		if (pMainForm) {
			pMainForm->addAudioFile(sPath);
			// Log this successful import operation...
			sDescription += tr("Audio file import \"%1\" on %2 %3.\n")
				.arg(QFileInfo(sPath).fileName())
				.arg(QDate::currentDate().toString("MMM dd yyyy"))
				.arg(QTime::currentTime().toString("hh:mm:ss"));
			pMainForm->appendMessages(
				tr("Audio file import: \"%1\".").arg(sPath));
		}
	}

	// Have we changed anything?
	bool bResult = false;
	if (iUpdate > 0) {
		// Log to session (undoable by import-track command)...
		pSession->setDescription(sDescription);
		// Put it in the form of an undoable command...
		bResult = pSession->execute(pImportTrackCommand);
	} else {
		delete pImportTrackCommand;
	}

	pSession->unlock();
	return bResult;
}


// Import MIDI files into new tracks...
bool qtractorTracks::addMidiTracks (
	QStringList files, unsigned long iClipStart )
{
	// Have we some?
	if (files.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	pSession->lock();

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand();

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// Needed to help on setting whole session properties
	// from the first imported MIDI file...
	int iImport = iTrack;
	unsigned long iTimeStart = pSession->tickFromFrame(iClipStart);

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// For each one of those files...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// This is one of the selected filenames....
		const QString& sPath = iter.next();
		// We'll be careful and pre-open the SMF header here...
		qtractorMidiFile file;
		if (!file.open(sPath))
			continue;
		// It all depends on the format...
		int iTracks = (file.format() == 1 ? file.tracks() : 16);
		for (int iTrackChannel = 0; iTrackChannel < iTracks; iTrackChannel++) {
			// Create a new track right away...
			const QColor& color = qtractorTrack::trackColor(++iTrack);
			qtractorTrack *pTrack
				= new qtractorTrack(pSession, qtractorTrack::Midi);
		//	pTrack->setTrackName(QFileInfo(sPath).baseName());
			pTrack->setBackground(color);
			pTrack->setForeground(color.darker());
			// Add the clip at once...
			qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
			pMidiClip->setFilename(sPath);
			pMidiClip->setTrackChannel(iTrackChannel);
			pMidiClip->setClipStart(iClipStart);
			if (iTrackChannel == 0 && iImport == 0)
				pMidiClip->setSessionFlag(true);
			// Time to add the new track/clip into session;
			// actuallly, this is when the given MIDI file and
			// track-channel gets open and read into the clip!
			pTrack->addClip(pMidiClip);
			// As far the standards goes,from which we'll strictly follow,
			// only the first track/channel has some tempo/time signature...
			if (iTrackChannel == 0) {
				// Some adjustment required...
				iImport++;
				iClipStart = pSession->frameFromTick(iTimeStart);
				pMidiClip->setClipStart(iClipStart);
			}
			// Time to check whether there is actual data on track...
			if (pMidiClip->clipLength() > 0) {
				// Add the new track to composite command...
				pTrack->setTrackName(pMidiClip->clipName());
				pTrack->setMidiChannel(pMidiClip->channel());
				pImportTrackCommand->addTrack(pTrack);
				iUpdate++;
				// Don't forget to add this one to local repository.
				if (pMainForm)
					pMainForm->addMidiFile(sPath);
			} else {
				// Get rid of these, now...
				pTrack->unlinkClip(pMidiClip);
				delete pMidiClip;
				delete pTrack;
			}
		}
		// Log this successful import operation...
		if (iUpdate > 0 && pMainForm) {
			sDescription += tr("MIDI file import \"%1\" on %2 %3.\n")
				.arg(QFileInfo(sPath).fileName())
				.arg(QDate::currentDate().toString("MMM dd yyyy"))
				.arg(QTime::currentTime().toString("hh:mm:ss"));
			pMainForm->appendMessages(
				tr("MIDI file import: \"%1\".").arg(sPath));
		}
	}

	// Have we changed anything?
	bool bResult = false;
	if (iUpdate > 0) {
		// Log to session (undoable by import-track command)...
		pSession->setDescription(sDescription);	
		// Put it in the form of an undoable command...
		bResult = pSession->execute(pImportTrackCommand);
	} else {
		delete pImportTrackCommand;
	}

	pSession->unlock();
	return bResult;
}


// Import MIDI file track-channel into new track...
bool qtractorTracks::addMidiTrackChannel ( const QString& sPath,
	int iTrackChannel, unsigned long iClipStart )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand();

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// Create a new track right away...
	const QColor& color = qtractorTrack::trackColor(++iTrack);
	qtractorTrack *pTrack
		= new qtractorTrack(pSession, qtractorTrack::Midi);
//	pTrack->setTrackName(QFileInfo(sPath).baseName());
	pTrack->setBackground(color);
	pTrack->setForeground(color.darker());
	// Add the clip at once...
	qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
	pMidiClip->setFilename(sPath);
	pMidiClip->setTrackChannel(iTrackChannel);
	pMidiClip->setClipStart(iClipStart);
	// Time to add the new track/clip into session...
	pTrack->addClip(pMidiClip);
	pTrack->setTrackName(pMidiClip->clipName());
	pTrack->setMidiChannel(pMidiClip->channel());
	// Add the new track to composite command...
	pImportTrackCommand->addTrack(pTrack);

	// Don't forget to add this one to local repository.
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		pMainForm->addMidiFile(sPath);
		// Log this successful import operation...
		sDescription += tr("MIDI file import \"%1\""
			" track-channel %2 on %3 %4.\n")
			.arg(QFileInfo(sPath).fileName()).arg(iTrackChannel)
			.arg(QDate::currentDate().toString("MMM dd yyyy"))
			.arg(QTime::currentTime().toString("hh:mm:ss"));
		pMainForm->appendMessages(
			tr("MIDI file import: \"%1\", track-channel: %2.")
			.arg(sPath).arg(iTrackChannel));
	}

	// Log to session (undoable by import-track command)...
	pSession->setDescription(sDescription);

	// Put it in the form of an undoable command...
	return pSession->execute(pImportTrackCommand);
}


// MIDI track/bus/channel alias active maintenance method.
void qtractorTracks::updateMidiTrack ( qtractorTrack *pMidiTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QString& sBusName = pMidiTrack->outputBusName();
	unsigned short iChannel = pMidiTrack->midiChannel();

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		// If same channel, force same bank/program stuff...
		if (pTrack != pMidiTrack
			&& pTrack->trackType() == qtractorTrack::Midi
			&& pTrack->outputBusName() == sBusName
			&& pTrack->midiChannel() == iChannel) {
			// Make else tracks MIDI attributes the same....
			pTrack->setMidiBankSelMethod(pMidiTrack->midiBankSelMethod());
			pTrack->setMidiBank(pMidiTrack->midiBank());
			pTrack->setMidiProgram(pMidiTrack->midiProgram());
			// Update the track list view, immediately...
			m_pTrackList->updateTrack(pTrack);
		}
	}

	// Update MIDI bus patch...
	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pMidiTrack->outputBus());
	if (pMidiBus == NULL)
		return;

	const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
	pMidiBus->setPatch(iChannel, patch.instrumentName,
		pMidiTrack->midiBankSelMethod(),
		pMidiTrack->midiBank(),
		pMidiTrack->midiProgram(),
		pMidiTrack);
}


// Simple main-form stabilizer redirector.
void qtractorTracks::selectionChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->selectionNotifySlot(NULL);
}


// Simple main-form dirty-flag redirector.
void qtractorTracks::contentsChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->changeNotifySlot(NULL);
}


// Overall contents reset.
void qtractorTracks::clear(void)
{
	m_pTrackList->clear();
	m_pTrackView->clear();

	updateContents(true);
}


// Track button notification.
void qtractorTracks::trackButtonSlot (
	qtractorTrackButton *pTrackButton, bool bOn )
{
	// Put it in the form of an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorTrackStateCommand(
			pTrackButton->track(), pTrackButton->toolType(), bOn));
}


// end of qtractorTracks.cpp
