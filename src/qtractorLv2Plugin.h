// qtractorLv2Plugin.h
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

#ifndef __qtractorLv2Plugin_h
#define __qtractorLv2Plugin_h

#include "qtractorPlugin.h"

#include <slv2/slv2.h>

#ifdef CONFIG_LV2_EVENT
// LV2 Event/MIDI support.
#include "lv2_event.h"
#include "lv2_event_helpers.h"
#define  QTRACTOR_LV2_MIDI_EVENT_ID 1
#endif

#if defined(CONFIG_LV2_GTK_UI) || defined(CONFIG_LV2_EXTERNAL_UI)
#define CONFIG_LV2_UI 1
// LV2 UI data/instance access support.
#include "lv2_data_access.h"
#include "lv2_instance_access.h"
#endif

#ifdef CONFIG_LV2_EXTERNAL_UI
// LV2 External UI support.
#include "lv2_external_ui.h"
#endif

#ifdef CONFIG_LV2_SAVERESTORE
// LV2 Save/Restore support.
#include "lv2_saverestore.h"
#endif

#ifdef CONFIG_LV2_PERSIST
// LV2 Persist support.
#include "lv2_persist.h"
#define QTRACTOR_LV2_ATOM_STRING_ID 2
#endif


//----------------------------------------------------------------------------
// qtractorLv2PluginType -- LV2 plugin type instance.
//

class qtractorLv2PluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorLv2PluginType(const QString& sUri, SLV2Plugin plugin = NULL)
		: qtractorPluginType(NULL, 0, qtractorPluginType::Lv2),
			m_sUri(sUri), m_slv2_plugin(plugin)	{}

	// Destructor.
	~qtractorLv2PluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Factory method (static)
	static qtractorLv2PluginType *createType(
		const QString& sUri, SLV2Plugin plugin = NULL);

	// LV2 plugin URI (virtual override).
	QString filename() const
		{ return m_sUri; }

	// LV2 descriptor method (static)
	static SLV2Plugin slv2_plugin(const QString& sUri);

	// Specific accessors.
	SLV2Plugin slv2_plugin() const
		{ return m_slv2_plugin; }

	// LV2 World stuff (ref. counted).
	static void slv2_open();
	static void slv2_close();

	// Plugin type listing (static).
	static bool getTypes(qtractorPluginPath& path);

protected:

	// LV2 plgin URI.
	QString    m_sUri;

	// LV2 descriptor itself.
	SLV2Plugin m_slv2_plugin;
};


//----------------------------------------------------------------------------
// qtractorLv2Plugin -- LV2 plugin instance.
//

class qtractorLv2Plugin : public qtractorPlugin
{
public:

	// Constructors.
	qtractorLv2Plugin(qtractorPluginList *pList,
		qtractorLv2PluginType *pLv2Type);

	// Destructor.
	~qtractorLv2Plugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Specific accessors.
	SLV2Plugin slv2_plugin() const;
	SLV2Instance slv2_instance(unsigned short iInstance) const;

	LV2_Handle lv2_handle(unsigned short iInstance) const;

	// Audio port numbers.
	unsigned long audioIn(unsigned short i)
		{ return m_piAudioIns[i]; }
	unsigned long audioOut(unsigned short i)
		{ return m_piAudioOuts[i]; }

#ifdef CONFIG_LV2_UI

	// GUI Editor stuff.
	void openEditor(QWidget *pParent);
	void closeEditor();
	void idleEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	void setEditorTitle(const QString& sTitle);

	// Parameter update method.
	void updateParam(qtractorPluginParam *pParam, float fValue);

	// Idle editor (static).
	static void idleEditorAll();

	// LV2 UI descriptor accessor.
	const LV2UI_Descriptor *lv2_ui_descriptor() const;

	// LV2 UI handle accessor.
	LV2UI_Handle lv2_ui_handle() const;

	// GUI editor closed state.
	void setEditorClosed(bool bClosed)
		{ m_bEditorClosed = bClosed; }
	bool isEditorClosed() const
		{ return m_bEditorClosed; }

#ifdef CONFIG_LV2_GTK_UI
	void closeEditorEx();
#endif

#endif

	// Configuration (restore) stuff.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state (save) snapshot.
	void freezeConfigs();

	// Plugin configuration/state (load) realization.
	void realizeConfigs();

	// Plugin configuration/state release.
	void releaseConfigs();

#ifdef CONFIG_LV2_SAVERESTORE

	// LV2 Save/Restore extension data descriptor accessor.
	const LV2SR_Descriptor *lv2_sr_descriptor(unsigned short iInstance) const;

	bool lv2_sr_save(unsigned short iInstance,
		const char *pszDirectory, LV2SR_File ***pppFiles);
	bool lv2_sr_restore(unsigned short iInstance,
		const LV2SR_File **ppFiles);

#endif

#ifdef CONFIG_LV2_PERSIST

	// LV2 Persist extension data descriptor accessor.
	const LV2_Persist *lv2_persist_descriptor(unsigned short iInstance) const;

	void lv2_persist_store(
		const char *key, const void *value, size_t size, uint32_t type);
	const void *lv2_persist_retrieve(
		const char *key, size_t *size, uint32_t *type);

#endif

protected:

	// Instance variables.
	SLV2Instance  *m_pInstances;

	// List of output control port indexes and data.
	unsigned long *m_piControlOuts;
	float         *m_pfControlOuts;
	float         *m_pfControlOutsLast;

	// List of audio port indexes.
	unsigned long *m_piAudioIns;
	unsigned long *m_piAudioOuts;

#ifdef CONFIG_LV2_EVENT
	// List of MIDI port indexes.
	unsigned long *m_piMidiIns;
#endif

#ifdef CONFIG_LV2_UI

	int            m_lv2_ui_type;

	QByteArray     m_aEditorTitle;
	bool           m_bEditorVisible;
	bool           m_bEditorClosed;

	SLV2UIs        m_slv2_uis;
	SLV2UI         m_slv2_ui;
	SLV2UIInstance m_slv2_ui_instance;

	LV2_Extension_Data_Feature m_lv2_data_access;

	LV2_Feature    m_lv2_data_access_feature;
	LV2_Feature    m_lv2_instance_access_feature;

	LV2_Feature    m_lv2_ui_feature;
	LV2_Feature  **m_lv2_ui_features;
	LV2UI_Widget   m_lv2_ui_widget;

#ifdef CONFIG_LV2_EXTERNAL_UI
	// Our own external UI host context.
	lv2_external_ui_host m_lv2_ui_external;
#endif

#ifdef CONFIG_LV2_GTK_UI
	// Our own GTK UI widget (parent frame).
	struct _GtkWidget *m_pGtkWindow;
#endif

#endif

#ifdef CONFIG_LV2_PERSIST
	QHash<QString, QByteArray> m_lv2_persist_configs;
#endif
};


//----------------------------------------------------------------------------
// qtractorLv2PluginParam -- LV2 plugin control input port instance.
//

class qtractorLv2PluginParam : public qtractorPluginParam
{
public:

	// Constructors.
	qtractorLv2PluginParam(qtractorLv2Plugin *pLv2Plugin,
		unsigned long iIndex);

	// Destructor.
	~qtractorLv2PluginParam();

	// Port range hints predicate methods.
	bool isBoundedBelow() const;
	bool isBoundedAbove() const;
	bool isDefaultValue() const;
	bool isLogarithmic() const;
	bool isSampleRate() const;
	bool isInteger() const;
	bool isToggled() const;
	bool isDisplay() const;

	// Current display value.
	QString display() const;

private:

	// Port bit-wise hints.
	enum {
		None        = 0,
		Toggled     = 1,
		Integer     = 2,
		SampleRate  = 4,
		Logarithmic = 8,
	};

	// Instance variables.
	unsigned int m_iPortHints;

	QHash<QString, QString> m_display;
};


#endif  // __qtractorLv2Plugin_h

// end of qtractorLv2Plugin.h
