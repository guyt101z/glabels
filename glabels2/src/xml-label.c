/*
 *  (GLABELS) Label and Business Card Creation program for GNOME
 *
 *  label.c:  GLabels xml label module
 *
 *  Copyright (C) 2001-2002  Jim Evins <evins@snaught.com>.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <config.h>

#include "xml-label.h"

#include <glib/gi18n.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "label.h"
#include "label-object.h"
#include "label-text.h"
#include "label-box.h"
#include "label-line.h"
#include "label-ellipse.h"
#include "label-image.h"
#include "label-barcode.h"
#include "base64.h"
#include "xml-label-04.h"
#include "xml-label-191.h"
#include <libglabels/template.h>
#include <libglabels/xml-template.h>
#include <libglabels/xml.h>
#include "util.h"

#include "debug.h"

/*========================================================*/
/* Private macros and constants.                          */
/*========================================================*/
#define COMPAT01_NAME_SPACE "http://snaught.com/glabels/0.1/"
#define COMPAT04_NAME_SPACE "http://snaught.com/glabels/0.4/"
#define COMPAT191_NAME_SPACE "http://snaught.com/glabels/1.92/"

/*========================================================*/
/* Private types.                                         */
/*========================================================*/

/*========================================================*/
/* Private globals.                                       */
/*========================================================*/

/*========================================================*/
/* Private function prototypes.                           */
/*========================================================*/

static glLabel       *xml_doc_to_label         (xmlDocPtr         doc,
						glXMLLabelStatus *status);

static glLabel       *xml_parse_label          (xmlNodePtr        root,
						glXMLLabelStatus *status);

static void           xml_parse_objects        (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_object_text    (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_object_box     (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_object_ellipse (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_object_line    (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_object_image   (xmlNodePtr        node,
					        glLabel          *label);

static void           xml_parse_object_barcode (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_merge_fields   (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_data           (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_pixdata        (xmlNodePtr        node,
						glLabel          *label);

static void           xml_parse_toplevel_span  (xmlNodePtr        node,
						glLabelObject    *object);


static xmlDocPtr      xml_label_to_doc         (glLabel          *label,
						glXMLLabelStatus *status);

static void           xml_create_objects       (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabel          *label);

static void           xml_create_object_text   (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabelObject    *object);

static void           xml_create_object_box    (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabelObject    *object);

static void           xml_create_object_line   (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabelObject    *object);

static void           xml_create_object_ellipse(xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabelObject    *object);

static void           xml_create_object_image  (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabelObject    *object);

static void           xml_create_object_barcode(xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabelObject    *object);

static void           xml_create_merge_fields  (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabel          *label);

static void           xml_create_data          (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabel          *label);

static void           xml_create_pixdata       (xmlNodePtr        root,
						xmlNsPtr          ns,
						glLabel          *label,
						gchar            *name);

static void           xml_create_toplevel_span (xmlNodePtr        node,
						xmlNsPtr          ns,
						glLabelText      *object_text);


/****************************************************************************/
/* Open and read label from xml file.                                       */
/****************************************************************************/
glLabel *
gl_xml_label_open (const gchar      *utf8_filename,
		   glXMLLabelStatus *status)
{
	xmlDocPtr  doc;
	glLabel   *label;
	gchar 	  *filename;

	gl_debug (DEBUG_XML, "START");

	filename = g_filename_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);
	g_return_val_if_fail (filename, NULL);

	doc = xmlParseFile (filename);
	if (!doc) {
		g_warning (_("xmlParseFile error"));
		*status = XML_LABEL_ERROR_OPEN_PARSE;
		return NULL;
	}

	xmlXIncludeProcess (doc);
	xmlReconciliateNs (doc, xmlDocGetRootElement (doc));

	label = xml_doc_to_label (doc, status);

	xmlFreeDoc (doc);

	if (label) {
		gl_label_set_filename (label, utf8_filename);
		gl_label_clear_modified (label);
	}

	g_free (filename);
	gl_debug (DEBUG_XML, "END");

	return label;
}

/****************************************************************************/
/* Read label from xml buffer.                                              */
/****************************************************************************/
glLabel *
gl_xml_label_open_buffer (const gchar      *buffer,
			  glXMLLabelStatus *status)
{
	xmlDocPtr  doc;
	glLabel   *label;

	gl_debug (DEBUG_XML, "START");

	doc = xmlParseDoc ((xmlChar *) buffer);
	if (!doc) {
		g_warning (_("xmlParseFile error"));
		*status = XML_LABEL_ERROR_OPEN_PARSE;
		return NULL;
	}

	label = xml_doc_to_label (doc, status);

	xmlFreeDoc (doc);

	if (label) {
		gl_label_clear_modified (label);
	}

	gl_debug (DEBUG_XML, "END");

	return label;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse xml doc structure and create label.                      */
/*--------------------------------------------------------------------------*/
static glLabel *
xml_doc_to_label (xmlDocPtr         doc,
		  glXMLLabelStatus *status)
{
	xmlNodePtr  root, node;
	xmlNsPtr    ns;
	glLabel    *label;

	gl_debug (DEBUG_XML, "START");

	LIBXML_TEST_VERSION;

	*status = XML_LABEL_OK;

	root = xmlDocGetRootElement (doc);
	if (!root || !root->name) {
		g_warning (_("No document root"));
		*status = XML_LABEL_ERROR_OPEN_PARSE;
		return NULL;
	}

	ns = xmlSearchNsByHref (doc, root, (xmlChar *)GL_XML_NAME_SPACE);
	if (ns != NULL) {
		label = xml_parse_label (root, status);
		if (label)
			gl_label_set_compression (label, xmlGetDocCompressMode (doc));
	} else {
		/* Try compatability mode 0.1 */
		ns = xmlSearchNsByHref (doc, root, (xmlChar *)COMPAT01_NAME_SPACE);
		if (ns != NULL)	{
			g_warning (_("Importing from glabels 0.1 format"));
			g_warning ("TODO");
			label = NULL; /* TODO */
		} else {
			/* Try compatability mode 0.4 */
			ns = xmlSearchNsByHref (doc, root,
						(xmlChar *)COMPAT04_NAME_SPACE);
			if (ns != NULL)	{
				g_warning (_("Importing from glabels 0.4 format"));
				label = gl_xml_label_04_parse (root, status);
			} else {
				/* Try compatability mode 1.91 */
				ns = xmlSearchNsByHref (doc, root,
							(xmlChar *)COMPAT191_NAME_SPACE);
				if (ns != NULL)	{
					g_warning (_("Importing from glabels 1.91 format"));
					label = gl_xml_label_191_parse (root, status);
				} else {
					g_warning (_("bad document, unknown glabels Namespace"));
					*status = XML_LABEL_ERROR_OPEN_PARSE;
					return NULL;
				}
			}
		}
	}

	gl_debug (DEBUG_XML, "END");

	return label;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse xml root node and create label.                          */
/*--------------------------------------------------------------------------*/
static glLabel *
xml_parse_label (xmlNodePtr        root,
		 glXMLLabelStatus *status)
{
	xmlNodePtr  child_node;
	glLabel    *label;
	glTemplate *template;

	gl_debug (DEBUG_XML, "START");

	*status = XML_LABEL_OK;

	if (!xmlStrEqual (root->name, (xmlChar *)"Glabels-document")) {
		g_warning (_("Bad root node = \"%s\""), root->name);
		*status = XML_LABEL_ERROR_OPEN_PARSE;
		return NULL;
	}

	label = GL_LABEL(gl_label_new ());

	/* Pass 1, extract data nodes to pre-load cache. */
	for (child_node = root->xmlChildrenNode; child_node != NULL; child_node = child_node->next) {
		if (xmlStrEqual (child_node->name, (xmlChar *)"Data")) {
			xml_parse_data (child_node, label);
		}
	}

	/* Pass 2, now extract everything else. */
	for (child_node = root->xmlChildrenNode;
             child_node != NULL;
	     child_node = child_node->next) {

		if (xmlStrEqual (child_node->name, (xmlChar *)"Template")) {
			template = gl_xml_template_parse_template_node (child_node);
			if (!template) {
				g_object_unref (label);
				*status = XML_LABEL_UNKNOWN_MEDIA;
				return NULL;
			}
			gl_template_register (template);
			gl_label_set_template (label, template);
			gl_template_free (template);
		} else if (xmlStrEqual (child_node->name, (xmlChar *)"Objects")) {
			xml_parse_objects (child_node, label);
		} else if (xmlStrEqual (child_node->name, (xmlChar *)"Merge")) {
			xml_parse_merge_fields (child_node, label);
		} else if (xmlStrEqual (child_node->name, (xmlChar *)"Data")) {
			/* Handled in pass 1. */
		} else {
			if (!xmlNodeIsText (child_node)) {
				g_warning (_("bad node in Document node =  \"%s\""),
					   child_node->name);
				g_object_unref (label);
				*status = XML_LABEL_ERROR_OPEN_PARSE;
				return NULL;
			}
		}
	}

	gl_debug (DEBUG_XML, "END");

	return label;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse Objects node.                                            */
/*--------------------------------------------------------------------------*/
static void
xml_parse_objects (xmlNodePtr  node,
		   glLabel    *label)
{
	gboolean    rotate_flag;
	xmlNodePtr  child;

	gl_debug (DEBUG_XML, "START");

	rotate_flag = gl_xml_get_prop_boolean (node, "rotate", FALSE);
	gl_label_set_rotate_flag (label, rotate_flag);

	for (child = node->xmlChildrenNode; child != NULL; child = child->next) {

		if (xmlStrEqual (child->name, (xmlChar *)"Object-text")) {
			xml_parse_object_text (child, label);
		} else if (xmlStrEqual (child->name, (xmlChar *)"Object-box")) {
			xml_parse_object_box (child, label);
		} else if (xmlStrEqual (child->name, (xmlChar *)"Object-ellipse")) {
			xml_parse_object_ellipse (child, label);
		} else if (xmlStrEqual (child->name, (xmlChar *)"Object-line")) {
			xml_parse_object_line (child, label);
		} else if (xmlStrEqual (child->name, (xmlChar *)"Object-image")) {
			xml_parse_object_image (child, label);
		} else if (xmlStrEqual (child->name, (xmlChar *)"Object-barcode")) {
			xml_parse_object_barcode (child, label);
		} else {
			if (!xmlNodeIsText (child)) {
				g_warning (_("bad node =  \"%s\""), child->name);
				break;
			}
		}
	}

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML Objects->Object-text Node                            */
/*--------------------------------------------------------------------------*/
static void
xml_parse_object_text (xmlNodePtr  node,
		       glLabel    *label)
{
	GObject          *object;
	gdouble           x, y;
	gdouble           w, h;
	xmlChar          *string;
	GtkJustification  just;
	gboolean          auto_shrink;
	gdouble           affine[6];
	xmlNodePtr        child;

	gl_debug (DEBUG_XML, "START");

	object = gl_label_text_new (label);

	/* position attrs */
	x = gl_xml_get_prop_length (node, "x", 0.0);
	y = gl_xml_get_prop_length (node, "y", 0.0);
	gl_label_object_set_position (GL_LABEL_OBJECT(object), x, y);

	/* implied size attrs */
	w = gl_xml_get_prop_length (node, "w", 0);
	h = gl_xml_get_prop_length (node, "h", 0);
	gl_label_object_set_size (GL_LABEL_OBJECT(object), w, h);

	/* justify attr */
	string = xmlGetProp (node, (xmlChar *)"justify");
	just = gl_util_string_to_just ((gchar *)string);
	xmlFree (string);
	gl_label_object_set_text_alignment (GL_LABEL_OBJECT(object), just);

	/* auto_shrink attr */
	auto_shrink = gl_xml_get_prop_boolean (node, "auto_shrink", FALSE);
	gl_label_text_set_auto_shrink (GL_LABEL_TEXT(object), auto_shrink);

	/* affine attrs */
	affine[0] = gl_xml_get_prop_double (node, "a0", 0.0);
	affine[1] = gl_xml_get_prop_double (node, "a1", 0.0);
	affine[2] = gl_xml_get_prop_double (node, "a2", 0.0);
	affine[3] = gl_xml_get_prop_double (node, "a3", 0.0);
	affine[4] = gl_xml_get_prop_double (node, "a4", 0.0);
	affine[5] = gl_xml_get_prop_double (node, "a5", 0.0);
	gl_label_object_set_affine (GL_LABEL_OBJECT(object), affine);

	/* Process children */
	for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
		if (xmlStrEqual (child->name,(xmlChar *) "Span")) {
			xml_parse_toplevel_span (child, GL_LABEL_OBJECT(object));
			break;
		} else {
			if (!xmlNodeIsText (child)) {
				g_warning ("Unexpected Object-text child: \"%s\"",
					   child->name);
			}
		}
	}

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML Objects->Object-box Node                             */
/*--------------------------------------------------------------------------*/
static void
xml_parse_object_box (xmlNodePtr  node,
		      glLabel    *label)
{
	GObject      *object;
	gdouble       x, y;
	gdouble       w, h;
	gdouble       line_width;
	glColorNode  *line_color_node;
	gdouble       affine[6];
	xmlChar      *string;
	glColorNode  *fill_color_node;

	gl_debug (DEBUG_XML, "START");

	object = gl_label_box_new (label);

	/* position attrs */
	x = gl_xml_get_prop_length (node, "x", 0.0);
	y = gl_xml_get_prop_length (node, "y", 0.0);
	gl_label_object_set_position (GL_LABEL_OBJECT(object), x, y);

	/* size attrs */
	w = gl_xml_get_prop_length (node, "w", 0);
	h = gl_xml_get_prop_length (node, "h", 0);
	gl_label_object_set_size (GL_LABEL_OBJECT(object), w, h);

	/* line attrs */
	line_width = gl_xml_get_prop_length (node, "line_width", 1.0);
	gl_label_object_set_line_width (GL_LABEL_OBJECT(object), line_width);
	
	line_color_node = gl_color_node_new_default ();
	string = xmlGetProp (node, (xmlChar *)"line_color_field");
	if ( string ) {
		line_color_node->field_flag = TRUE;
		line_color_node->key = g_strdup ((gchar *)string);
		xmlFree (string);
	} else {
		line_color_node->color = gl_xml_get_prop_uint (node, "line_color", 0);
	}
	gl_label_object_set_line_color (GL_LABEL_OBJECT(object), line_color_node);
	gl_color_node_free (&line_color_node);


	/* fill attrs */
	fill_color_node = gl_color_node_new_default ();
	string = xmlGetProp (node, (xmlChar *)"fill_color_field");
	if ( string ) {
		fill_color_node->field_flag = TRUE;
		fill_color_node->key = g_strdup ((gchar *)string);
	} else {
		fill_color_node->color = gl_xml_get_prop_uint (node, "fill_color", 0);
	}
	gl_label_object_set_fill_color (GL_LABEL_OBJECT(object), fill_color_node);
	gl_color_node_free (&fill_color_node);
	
	/* affine attrs */
	affine[0] = gl_xml_get_prop_double (node, "a0", 0.0);
	affine[1] = gl_xml_get_prop_double (node, "a1", 0.0);
	affine[2] = gl_xml_get_prop_double (node, "a2", 0.0);
	affine[3] = gl_xml_get_prop_double (node, "a3", 0.0);
	affine[4] = gl_xml_get_prop_double (node, "a4", 0.0);
	affine[5] = gl_xml_get_prop_double (node, "a5", 0.0);
	gl_label_object_set_affine (GL_LABEL_OBJECT(object), affine);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML Objects->Object-ellipse Node                         */
/*--------------------------------------------------------------------------*/
static void
xml_parse_object_ellipse (xmlNodePtr  node,
			  glLabel    *label)
{
	GObject     *object;
	gdouble      x, y;
	gdouble      w, h;
	gdouble      line_width;
	glColorNode *line_color_node;
	gdouble      affine[6];
	xmlChar     *string;
	glColorNode *fill_color_node;

	gl_debug (DEBUG_XML, "START");

	object = gl_label_ellipse_new (label);

	/* position attrs */
	x = gl_xml_get_prop_length (node, "x", 0.0);
	y = gl_xml_get_prop_length (node, "y", 0.0);
	gl_label_object_set_position (GL_LABEL_OBJECT(object), x, y);

	/* size attrs */
	w = gl_xml_get_prop_length (node, "w", 0);
	h = gl_xml_get_prop_length (node, "h", 0);
	gl_label_object_set_size (GL_LABEL_OBJECT(object), w, h);

	/* line attrs */
	line_width = gl_xml_get_prop_length (node, "line_width", 1.0);
	gl_label_object_set_line_width (GL_LABEL_OBJECT(object), line_width);

	line_color_node = gl_color_node_new_default ();
	string = xmlGetProp (node, (xmlChar *)"line_color_field");
	if ( string ) {
		line_color_node->field_flag = TRUE;
		line_color_node->key = g_strdup ((gchar *)string);
		xmlFree (string);
	} else {
		line_color_node->color = gl_xml_get_prop_uint (node, "line_color", 0);		
	}
	gl_label_object_set_line_color (GL_LABEL_OBJECT(object), line_color_node);
	gl_color_node_free (&line_color_node);


	/* fill attrs */
	fill_color_node = gl_color_node_new_default ();
	string = xmlGetProp (node, (xmlChar *)"fill_color_field");
	if ( string ) {
		fill_color_node->field_flag = TRUE;
		fill_color_node->key = g_strdup ((gchar *)string);
		xmlFree (string);
	} else {
		fill_color_node->color = gl_xml_get_prop_uint (node, "fill_color", 0);
	}
	gl_label_object_set_fill_color (GL_LABEL_OBJECT(object), fill_color_node);
	gl_color_node_free (&fill_color_node);

	/* affine attrs */
	affine[0] = gl_xml_get_prop_double (node, "a0", 0.0);
	affine[1] = gl_xml_get_prop_double (node, "a1", 0.0);
	affine[2] = gl_xml_get_prop_double (node, "a2", 0.0);
	affine[3] = gl_xml_get_prop_double (node, "a3", 0.0);
	affine[4] = gl_xml_get_prop_double (node, "a4", 0.0);
	affine[5] = gl_xml_get_prop_double (node, "a5", 0.0);
	gl_label_object_set_affine (GL_LABEL_OBJECT(object), affine);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML Objects->Object-line Node                            */
/*--------------------------------------------------------------------------*/
static void
xml_parse_object_line (xmlNodePtr  node,
		       glLabel    *label)
{
	GObject     *object;
	gdouble      x, y;
	gdouble      dx, dy;
	gdouble      line_width;
	glColorNode *line_color_node;
	gdouble      affine[6];
	xmlChar     *string;

	gl_debug (DEBUG_XML, "START");

	object = gl_label_line_new (label);

	/* position attrs */
	x = gl_xml_get_prop_length (node, "x", 0.0);
	y = gl_xml_get_prop_length (node, "y", 0.0);
	gl_label_object_set_position (GL_LABEL_OBJECT(object), x, y);

	/* length attrs */
	dx = gl_xml_get_prop_length (node, "dx", 0);
	dy = gl_xml_get_prop_length (node, "dy", 0);
	gl_label_object_set_size (GL_LABEL_OBJECT(object), dx, dy);

	/* line attrs */
	line_width = gl_xml_get_prop_length (node, "line_width", 1.0);
	gl_label_object_set_line_width (GL_LABEL_OBJECT(object), line_width);
	
	line_color_node = gl_color_node_new_default ();
	string = xmlGetProp (node, (xmlChar *)"line_color_field");
	if ( string ) {
		line_color_node->field_flag = TRUE;
		line_color_node->key = g_strdup ((gchar *)string);
		xmlFree (string);
	} else {
		line_color_node->color = gl_xml_get_prop_uint (node, "line_color", 0);		
	}
	gl_label_object_set_line_color (GL_LABEL_OBJECT(object), line_color_node);
	gl_color_node_free (&line_color_node);

	/* affine attrs */
	affine[0] = gl_xml_get_prop_double (node, "a0", 0.0);
	affine[1] = gl_xml_get_prop_double (node, "a1", 0.0);
	affine[2] = gl_xml_get_prop_double (node, "a2", 0.0);
	affine[3] = gl_xml_get_prop_double (node, "a3", 0.0);
	affine[4] = gl_xml_get_prop_double (node, "a4", 0.0);
	affine[5] = gl_xml_get_prop_double (node, "a5", 0.0);
	gl_label_object_set_affine (GL_LABEL_OBJECT(object), affine);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML Objects->Object-image Node                           */
/*--------------------------------------------------------------------------*/
static void
xml_parse_object_image (xmlNodePtr  node,
			glLabel    *label)
{
	GObject      *object;
	gdouble       x, y;
	gdouble       w, h;
	xmlChar      *string;
	glTextNode   *filename;
	gdouble       affine[6];

	gl_debug (DEBUG_XML, "START");

	object = gl_label_image_new (label);

	/* position attrs */
	x = gl_xml_get_prop_length (node, "x", 0.0);
	y = gl_xml_get_prop_length (node, "y", 0.0);
	gl_label_object_set_position (GL_LABEL_OBJECT(object), x, y);

	/* src or field attr */
	string = xmlGetProp (node, (xmlChar *)"src");
	if ( string ) {
		filename = g_new0 (glTextNode, 1);
		filename->field_flag = FALSE;
		filename->data = g_strdup ((gchar *)string);
		gl_label_image_set_filename (GL_LABEL_IMAGE(object), filename);
		gl_text_node_free (&filename);
		xmlFree (string);
	} else {
		string = xmlGetProp (node, (xmlChar *)"field");
		if ( string ) {
			filename = g_new0 (glTextNode, 1);
			filename->field_flag = TRUE;
			filename->data = g_strdup ((gchar *)string);
			gl_label_image_set_filename (GL_LABEL_IMAGE(object), filename);
			gl_text_node_free (&filename);
			xmlFree (string);
		} else {
			g_warning ("Missing Object-image src or field attr");
		}
	}

	/* size attrs */
	w = gl_xml_get_prop_length (node, "w", 0);
	h = gl_xml_get_prop_length (node, "h", 0);
	gl_label_object_set_size (GL_LABEL_OBJECT(object), w, h);

	/* affine attrs */
	affine[0] = gl_xml_get_prop_double (node, "a0", 0.0);
	affine[1] = gl_xml_get_prop_double (node, "a1", 0.0);
	affine[2] = gl_xml_get_prop_double (node, "a2", 0.0);
	affine[3] = gl_xml_get_prop_double (node, "a3", 0.0);
	affine[4] = gl_xml_get_prop_double (node, "a4", 0.0);
	affine[5] = gl_xml_get_prop_double (node, "a5", 0.0);
	gl_label_object_set_affine (GL_LABEL_OBJECT(object), affine);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML Objects->Object-barcode Node                         */
/*--------------------------------------------------------------------------*/
static void
xml_parse_object_barcode (xmlNodePtr  node,
			  glLabel    *label)
{
	GObject            *object;
	gdouble             x, y;
	gdouble             w, h;
	xmlChar            *string;
	glTextNode         *text_node;
	xmlChar            *id;
	gboolean            text_flag;
	gboolean            checksum_flag;
	glColorNode        *color_node;
	guint               format_digits;
	gdouble             affine[6];

	gl_debug (DEBUG_XML, "START");

	object = gl_label_barcode_new (label);

	/* position attrs */
	x = gl_xml_get_prop_length (node, "x", 0.0);
	y = gl_xml_get_prop_length (node, "y", 0.0);
	gl_label_object_set_position (GL_LABEL_OBJECT(object), x, y);

	/* size attrs */
	w = gl_xml_get_prop_length (node, "w", 0);
	h = gl_xml_get_prop_length (node, "h", 0);
	gl_label_object_set_size (GL_LABEL_OBJECT(object), w, h);

	/* prop attrs */
	id = xmlGetProp (node, (xmlChar *)"style");
	text_flag = gl_xml_get_prop_boolean (node, "text", FALSE);
	checksum_flag = gl_xml_get_prop_boolean (node, "checksum", TRUE);
	format_digits = gl_xml_get_prop_uint (node, "format", 10);
	gl_label_barcode_set_props (GL_LABEL_BARCODE(object),
				    (gchar *)id, text_flag, checksum_flag, format_digits);
	xmlFree (id);
	
	color_node = gl_color_node_new_default ();
	string = xmlGetProp (node, (xmlChar *)"color_field");
	if ( string ) {
		color_node->field_flag = TRUE;
		color_node->key = g_strdup ((gchar *)string);
		xmlFree (string);
	} else {
		color_node->color = gl_xml_get_prop_uint (node, "color", 0);		
	}
	gl_label_object_set_line_color (GL_LABEL_OBJECT(object), color_node);
	gl_color_node_free (&color_node);

	/* data or field attr */
	string = xmlGetProp (node, (xmlChar *)"data");
	if ( string ) {
		text_node = g_new0 (glTextNode, 1);
		text_node->field_flag = FALSE;
		text_node->data = g_strdup ((gchar *)string);
		gl_label_barcode_set_data (GL_LABEL_BARCODE(object), text_node);
		gl_text_node_free (&text_node);
		xmlFree (string);
	} else {
		string = xmlGetProp (node, (xmlChar *)"field");
		if ( string ) {
			text_node = g_new0 (glTextNode, 1);
			text_node->field_flag = TRUE;
			text_node->data = g_strdup ((gchar *)string);
			gl_label_barcode_set_data (GL_LABEL_BARCODE(object), text_node);
			gl_text_node_free (&text_node);
			xmlFree (string);
		} else {
			g_warning ("Missing Object-barcode data or field attr");
		}
	}

	/* affine attrs */
	affine[0] = gl_xml_get_prop_double (node, "a0", 0.0);
	affine[1] = gl_xml_get_prop_double (node, "a1", 0.0);
	affine[2] = gl_xml_get_prop_double (node, "a2", 0.0);
	affine[3] = gl_xml_get_prop_double (node, "a3", 0.0);
	affine[4] = gl_xml_get_prop_double (node, "a4", 0.0);
	affine[5] = gl_xml_get_prop_double (node, "a5", 0.0);
	gl_label_object_set_affine (GL_LABEL_OBJECT(object), affine);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML merge fields tag.                                    */
/*--------------------------------------------------------------------------*/
static void
xml_parse_merge_fields (xmlNodePtr  node,
			glLabel    *label)
{
	xmlNodePtr  child;
	xmlChar    *string;
	glMerge    *merge;

	gl_debug (DEBUG_XML, "START");

	string = xmlGetProp (node, (xmlChar *)"type");
	merge = gl_merge_new ((gchar *)string);
	xmlFree (string);

	string = xmlGetProp (node, (xmlChar *)"src");
	gl_merge_set_src (merge, (gchar *)string);
	xmlFree (string);

	gl_label_set_merge (label, merge);

	g_object_unref (G_OBJECT(merge));

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML data tag.                                            */
/*--------------------------------------------------------------------------*/
static void
xml_parse_data (xmlNodePtr  node,
		glLabel    *label)
{
	xmlNodePtr  child;

	gl_debug (DEBUG_XML, "START");

	for (child = node->xmlChildrenNode; child != NULL; child = child->next) {

		if (xmlStrEqual (child->name, (xmlChar *)"Pixdata")) {
			xml_parse_pixdata (child, label);
		} else {
			if (!xmlNodeIsText (child)) {
				g_warning (_("bad node in Data node =  \"%s\""),
					   child->name);
			}
		}
	}

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse XML pixbuf data tag.                                     */
/*--------------------------------------------------------------------------*/
static void
xml_parse_pixdata (xmlNodePtr  node,
		   glLabel    *label)
{
	xmlChar    *name, *base64;
	guchar     *stream;
	guint       stream_length;
	gboolean    ret;
	GdkPixdata *pixdata;
	GdkPixbuf  *pixbuf;
	GHashTable *pixbuf_cache;

	gl_debug (DEBUG_XML, "START");

	name = xmlGetProp (node, (xmlChar *)"name");
	base64 = xmlNodeGetContent (node);

	stream = gl_base64_decode ((gchar *)base64, &stream_length);
	pixdata = g_new0 (GdkPixdata, 1);
	ret = gdk_pixdata_deserialize (pixdata, stream_length, stream, NULL);

	if (ret) {
		pixbuf = gdk_pixbuf_from_pixdata (pixdata, TRUE, NULL);

		pixbuf_cache = gl_label_get_pixbuf_cache (label);
		gl_pixbuf_cache_add_pixbuf (pixbuf_cache, (gchar *)name, pixbuf);
	}

	xmlFree (name);
	xmlFree (base64);

	g_free (stream);
	g_free (pixdata);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Parse top-level Span tag.                                      */
/*--------------------------------------------------------------------------*/
static void
xml_parse_toplevel_span  (xmlNodePtr        node,
			  glLabelObject    *object)
{
	xmlChar          *font_family;
	gdouble           font_size;
	GnomeFontWeight   font_weight;
	gboolean          font_italic_flag;
	glColorNode      *color_node;
	gdouble           text_line_spacing;
	xmlChar          *string;
	GList            *lines, *text_nodes;
	xmlNodePtr        child;
	glTextNode       *text_node;

	gl_debug (DEBUG_XML, "START");

	/* Font family attr */
	font_family = xmlGetProp (node, (xmlChar *)"font_family");
	gl_label_object_set_font_family (object, (gchar *)font_family);
	xmlFree (font_family);

	/* Font size attr */
	font_size = gl_xml_get_prop_double (node, "font_size", 0.0);
	gl_label_object_set_font_size (object, font_size);

	/* Font weight attr */
	string = xmlGetProp (node, (xmlChar *)"font_weight");
	font_weight = gl_util_string_to_weight ((gchar *)string);
	xmlFree (string);
	gl_label_object_set_font_weight (object, font_weight);

	/* Font italic flag attr */
	font_italic_flag = gl_xml_get_prop_boolean (node, "font_italic", FALSE);
	gl_label_object_set_font_italic_flag (object, font_italic_flag);

	/* Text color attr */
	color_node = gl_color_node_new_default ();
	string = xmlGetProp (node, (xmlChar *)"color_field");
	if ( string ) {
		color_node->field_flag = TRUE;
		color_node->key = g_strdup ((gchar *)string);
		xmlFree (string);
	} else {
		color_node->color = gl_xml_get_prop_uint (node, "color", 0);		
	}
	gl_label_object_set_text_color (object, color_node);
	gl_color_node_free (&color_node);
	

	/* Text line spacing attr  */
	text_line_spacing = gl_xml_get_prop_double (node, "line_spacing", 1.0);
	gl_label_object_set_text_line_spacing (object, text_line_spacing); 

	/* Now descend children, and build lines of text nodes */
	lines = NULL;
	text_nodes = NULL;
	for (child = node->xmlChildrenNode; child != NULL; child = child->next) {

		if (xmlNodeIsText (child)) {
			xmlChar *data = xmlNodeGetContent (child); 

			/* Hack: if the first char is LF, it's a xml formatting string */
			if (data[0] != '\n') { 
				/* Literal text */
				text_node = g_new0 (glTextNode, 1);
				text_node->field_flag = FALSE;
				text_node->data = g_strdup ((gchar *)data);
				text_nodes = g_list_append (text_nodes, text_node);
			}
			xmlFree (data);

		} else if (xmlStrEqual (child->name, (xmlChar *)"Span")) {

			g_warning ("Unexpected rich text (not supported, yet!)");

		} else if (xmlStrEqual (child->name, (xmlChar *)"Field")) {

			/* Field node */
			string = xmlGetProp (child, (xmlChar *)"name");
			text_node = g_new0 (glTextNode, 1);
			text_node->field_flag = TRUE;
			text_node->data = g_strdup ((gchar *)string);
			text_nodes = g_list_append (text_nodes, text_node);
			xmlFree (string);

		} else if (xmlStrEqual (child->name, (xmlChar *)"NL")) {

			/* Store line. */
			lines = g_list_append (lines, text_nodes);
			text_nodes = NULL;

		} else {
			g_warning ("Unexpected Span child: \"%s\"", child->name);
		}

	}
	if ( text_nodes ) {
		/* Store last line. */
		lines = g_list_append (lines, text_nodes);
		text_nodes = NULL;
	}
	gl_label_text_set_lines (GL_LABEL_TEXT(object), lines);
	gl_text_node_lines_free (&lines);

	gl_debug (DEBUG_XML, "END");
}

/****************************************************************************/
/* Save label to xml label file.                                            */
/****************************************************************************/
void
gl_xml_label_save (glLabel          *label,
		   const gchar      *utf8_filename,
		   glXMLLabelStatus *status)
{
	xmlDocPtr doc;
	gint      xml_ret;
	gchar 	  *filename;

	gl_debug (DEBUG_XML, "START");

	doc = xml_label_to_doc (label, status);

	filename = g_filename_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);
	if (!filename)
		g_warning (_("Utf8 conversion error."));
	else {
		xmlSetDocCompressMode (doc, gl_label_get_compression (label));
		xml_ret = xmlSaveFormatFile (filename, doc, TRUE);
		xmlFreeDoc (doc);
		if (xml_ret == -1) {

			g_warning (_("Problem saving xml file."));
			*status = XML_LABEL_ERROR_SAVE_FILE;

		} else {

			gl_label_set_filename (label, utf8_filename);
			gl_label_clear_modified (label);

		}
		g_free (filename);
	}

	gl_debug (DEBUG_XML, "END");
}

/****************************************************************************/
/* Save label to xml buffer.                                                */
/****************************************************************************/
gchar *
gl_xml_label_save_buffer (glLabel          *label,
			  glXMLLabelStatus *status)
{
	xmlDocPtr  doc;
	gint       size;
	gchar     *buffer;

	gl_debug (DEBUG_XML, "START");

	doc = xml_label_to_doc (label, status);

	xmlDocDumpMemory (doc, (xmlChar **)&buffer, &size);
	xmlFreeDoc (doc);

	gl_label_clear_modified (label);

	gl_debug (DEBUG_XML, "END");

	return buffer;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Convert label to xml doc structure.                            */
/*--------------------------------------------------------------------------*/
static xmlDocPtr
xml_label_to_doc (glLabel          *label,
		  glXMLLabelStatus *status)
{
	xmlDocPtr   doc;
	xmlNsPtr    ns;
	glTemplate *template;
	glMerge    *merge;

	gl_debug (DEBUG_XML, "START");

	LIBXML_TEST_VERSION;

	doc = xmlNewDoc ((xmlChar *)"1.0");
	doc->xmlRootNode = xmlNewDocNode (doc, NULL, (xmlChar *)"Glabels-document", NULL);

	ns = xmlNewNs (doc->xmlRootNode, (xmlChar *)GL_XML_NAME_SPACE, NULL);
	xmlSetNs (doc->xmlRootNode, ns);

	template = gl_label_get_template (label);
	gl_xml_template_create_template_node (template, doc->xmlRootNode, ns);

	xml_create_objects (doc->xmlRootNode, ns, label);

	merge = gl_label_get_merge (label);
	gl_debug (DEBUG_XML, "merge=%p", merge);
	if (merge != NULL) {
		xml_create_merge_fields (doc->xmlRootNode, ns, label);
		g_object_unref (G_OBJECT(merge));
	}

	xml_create_data (doc->xmlRootNode, ns, label);

	gl_debug (DEBUG_XML, "END");

	*status = XML_LABEL_OK;
	return doc;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Objects Node                                           */
/*--------------------------------------------------------------------------*/
static void
xml_create_objects (xmlNodePtr  root,
		    xmlNsPtr    ns,
		    glLabel    *label)
{
	xmlNodePtr     node;
	gboolean       rotate_flag;
	GList         *p;
	glLabelObject *object;

	gl_debug (DEBUG_XML, "START");

	rotate_flag = gl_label_get_rotate_flag (label);

	node = xmlNewChild (root, ns, (xmlChar *)"Objects", NULL);
	xmlSetProp (node, (xmlChar *)"id", (xmlChar *)"0");
	gl_xml_set_prop_boolean (node, "rotate", rotate_flag);

	for (p = label->objects; p != NULL; p = p->next) {

		object = GL_LABEL_OBJECT(p->data);

		if ( GL_IS_LABEL_TEXT(object) ) {
			xml_create_object_text (node, ns, object);
		} else if ( GL_IS_LABEL_BOX(object) ) {
			xml_create_object_box (node, ns, object);
		} else if ( GL_IS_LABEL_ELLIPSE(object) ) {
			xml_create_object_ellipse (node, ns, object);
		} else if ( GL_IS_LABEL_LINE(object) ) {
			xml_create_object_line (node, ns, object);
		} else if ( GL_IS_LABEL_IMAGE(object) ) {
			xml_create_object_image (node, ns, object);
		} else if ( GL_IS_LABEL_BARCODE(object) ) {
			xml_create_object_barcode (node, ns, object);
		} else {
			g_warning ("Unknown label object");
		}

	}

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Objects->Object-text Node                              */
/*--------------------------------------------------------------------------*/
static void
xml_create_object_text (xmlNodePtr     root,
			xmlNsPtr       ns,
			glLabelObject *object)
{
	xmlNodePtr        node;
	gdouble           x, y;
	gdouble           w, h;
	GtkJustification  just;
	gboolean          auto_shrink;
	gdouble           affine[6];

	gl_debug (DEBUG_XML, "START");

	node = xmlNewChild (root, ns, (xmlChar *)"Object-text", NULL);

	/* position attrs */
	gl_label_object_get_position (object, &x, &y);
	gl_xml_set_prop_length (node, "x", x);
	gl_xml_set_prop_length (node, "y", y);

	/* size attrs */
	gl_label_text_get_box ( GL_LABEL_TEXT(object), &w, &h);
	gl_xml_set_prop_length (node, "w", w);
	gl_xml_set_prop_length (node, "h", h);

	/* justify attr */
	just = gl_label_object_get_text_alignment (object);
	xmlSetProp (node, (xmlChar *)"justify", (xmlChar *)gl_util_just_to_string (just));

	/* auto_shrink attr */
	auto_shrink = gl_label_text_get_auto_shrink (GL_LABEL_TEXT (object));
	gl_xml_set_prop_boolean (node, "auto_shrink", auto_shrink);

	/* affine attrs */
	gl_label_object_get_affine (object, affine);
	gl_xml_set_prop_double (node, "a0", affine[0]);
	gl_xml_set_prop_double (node, "a1", affine[1]);
	gl_xml_set_prop_double (node, "a2", affine[2]);
	gl_xml_set_prop_double (node, "a3", affine[3]);
	gl_xml_set_prop_double (node, "a4", affine[4]);
	gl_xml_set_prop_double (node, "a5", affine[5]);

	/* Add children */
	xml_create_toplevel_span (node, ns, GL_LABEL_TEXT(object));

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Objects->Object-box Node                               */
/*--------------------------------------------------------------------------*/
static void
xml_create_object_box (xmlNodePtr     root,
		       xmlNsPtr       ns,
		       glLabelObject *object)
{
	xmlNodePtr        node;
	gdouble           x, y;
	gdouble           w, h;
	gdouble           line_width;
	glColorNode      *line_color_node;
	gdouble           affine[6];
	glColorNode *fill_color_node;

	gl_debug (DEBUG_XML, "START");

	node = xmlNewChild (root, ns, (xmlChar *)"Object-box", NULL);

	/* position attrs */
	gl_label_object_get_position (object, &x, &y);
	gl_xml_set_prop_length (node, "x", x);
	gl_xml_set_prop_length (node, "y", y);

	/* size attrs */
	gl_label_object_get_size (object, &w, &h);
	gl_xml_set_prop_length (node, "w", w);
	gl_xml_set_prop_length (node, "h", h);

	/* line attrs */
	line_width = gl_label_object_get_line_width (GL_LABEL_OBJECT(object));
	gl_xml_set_prop_length (node, "line_width", line_width);
	
	line_color_node = gl_label_object_get_line_color (GL_LABEL_OBJECT(object));
	if (line_color_node->field_flag)
	{
		xmlSetProp (node, (xmlChar *)"line_color_field", (xmlChar *)line_color_node->key);
	}
	else
	{
		gl_xml_set_prop_uint_hex (node, "line_color", line_color_node->color);
	}
	gl_color_node_free (&line_color_node);

	/* fill attrs (color or field) */
	fill_color_node = gl_label_object_get_fill_color (GL_LABEL_OBJECT(object));
	if (fill_color_node->field_flag)
	{
		xmlSetProp (node, (xmlChar *)"fill_color_field", (xmlChar *)fill_color_node->key);
	}
	else
	{
		gl_xml_set_prop_uint_hex (node, "fill_color", fill_color_node->color);
	}
	gl_color_node_free (&fill_color_node);

	/* affine attrs */
	gl_label_object_get_affine (object, affine);
	gl_xml_set_prop_double (node, "a0", affine[0]);
	gl_xml_set_prop_double (node, "a1", affine[1]);
	gl_xml_set_prop_double (node, "a2", affine[2]);
	gl_xml_set_prop_double (node, "a3", affine[3]);
	gl_xml_set_prop_double (node, "a4", affine[4]);
	gl_xml_set_prop_double (node, "a5", affine[5]);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Objects->Object-ellipse Node                           */
/*--------------------------------------------------------------------------*/
static void
xml_create_object_ellipse (xmlNodePtr     root,
			   xmlNsPtr       ns,
			   glLabelObject *object)
{
	xmlNodePtr        node;
	gdouble           x, y;
	gdouble           w, h;
	gdouble           line_width;
	glColorNode      *line_color_node;
	gdouble           affine[6];
	glColorNode *fill_color_node;

	gl_debug (DEBUG_XML, "START");

	node = xmlNewChild (root, ns, (xmlChar *)"Object-ellipse", NULL);

	/* position attrs */
	gl_label_object_get_position (object, &x, &y);
	gl_xml_set_prop_length (node, "x", x);
	gl_xml_set_prop_length (node, "y", y);

	/* size attrs */
	gl_label_object_get_size (object, &w, &h);
	gl_xml_set_prop_length (node, "w", w);
	gl_xml_set_prop_length (node, "h", h);

	/* line attrs */
	line_width = gl_label_object_get_line_width (GL_LABEL_OBJECT(object));
	gl_xml_set_prop_length (node, "line_width", line_width);
	
	line_color_node = gl_label_object_get_line_color (GL_LABEL_OBJECT(object));
	if (line_color_node->field_flag)
	{
		xmlSetProp (node, (xmlChar *)"line_color_field", (xmlChar *)line_color_node->key);
	}
	else
	{
		gl_xml_set_prop_uint_hex (node, "line_color", line_color_node->color);
	}
	gl_color_node_free (&line_color_node);


	/* fill attrs (color or field) */
	fill_color_node = gl_label_object_get_fill_color (GL_LABEL_OBJECT(object));
	if (fill_color_node->field_flag)
	{
		xmlSetProp (node, (xmlChar *)"fill_color_field", (xmlChar *)fill_color_node->key);
	}
	else
	{
		gl_xml_set_prop_uint_hex (node, "fill_color", fill_color_node->color);
	}
	gl_color_node_free (&fill_color_node);

	/* affine attrs */
	gl_label_object_get_affine (object, affine);
	gl_xml_set_prop_double (node, "a0", affine[0]);
	gl_xml_set_prop_double (node, "a1", affine[1]);
	gl_xml_set_prop_double (node, "a2", affine[2]);
	gl_xml_set_prop_double (node, "a3", affine[3]);
	gl_xml_set_prop_double (node, "a4", affine[4]);
	gl_xml_set_prop_double (node, "a5", affine[5]);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Objects->Object-line Node                              */
/*--------------------------------------------------------------------------*/
static void
xml_create_object_line (xmlNodePtr     root,
			xmlNsPtr       ns,
			glLabelObject *object)
{
	xmlNodePtr        node;
	gdouble           x, y;
	gdouble           dx, dy;
	gdouble           line_width;
	glColorNode      *line_color_node;
	gdouble           affine[6];

	gl_debug (DEBUG_XML, "START");

	node = xmlNewChild (root, ns, (xmlChar *)"Object-line", NULL);

	/* position attrs */
	gl_label_object_get_position (object, &x, &y);
	gl_xml_set_prop_length (node, "x", x);
	gl_xml_set_prop_length (node, "y", y);

	/* length attrs */
	gl_label_object_get_size (object, &dx, &dy);
	gl_xml_set_prop_length (node, "dx", dx);
	gl_xml_set_prop_length (node, "dy", dy);

	/* line attrs */
	line_width = gl_label_object_get_line_width (GL_LABEL_OBJECT(object));
	gl_xml_set_prop_length (node, "line_width", line_width);
	
	line_color_node = gl_label_object_get_line_color (GL_LABEL_OBJECT(object));
	if (line_color_node->field_flag)
	{
		xmlSetProp (node, (xmlChar *)"line_color_field", (xmlChar *)line_color_node->key);
	}
	else
	{
		gl_xml_set_prop_uint_hex (node, "line_color", line_color_node->color);
	}
	gl_color_node_free (&line_color_node);


	/* affine attrs */
	gl_label_object_get_affine (object, affine);
	gl_xml_set_prop_double (node, "a0", affine[0]);
	gl_xml_set_prop_double (node, "a1", affine[1]);
	gl_xml_set_prop_double (node, "a2", affine[2]);
	gl_xml_set_prop_double (node, "a3", affine[3]);
	gl_xml_set_prop_double (node, "a4", affine[4]);
	gl_xml_set_prop_double (node, "a5", affine[5]);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Objects->Object-image Node                             */
/*--------------------------------------------------------------------------*/
static void
xml_create_object_image (xmlNodePtr     root,
			 xmlNsPtr       ns,
			 glLabelObject *object)
{
	xmlNodePtr        node;
	gdouble           x, y;
	gdouble           w, h;
	glTextNode       *filename;
	gdouble           affine[6];

	gl_debug (DEBUG_XML, "START");

	node = xmlNewChild (root, ns, (xmlChar *)"Object-image", NULL);

	/* position attrs */
	gl_label_object_get_position (object, &x, &y);
	gl_xml_set_prop_length (node, "x", x);
	gl_xml_set_prop_length (node, "y", y);

	/* size attrs */
	gl_label_object_get_size (object, &w, &h);
	gl_xml_set_prop_length (node, "w", w);
	gl_xml_set_prop_length (node, "h", h);

	/* src OR field attr */
	filename = gl_label_image_get_filename (GL_LABEL_IMAGE(object));
	if (filename->field_flag) {
		xmlSetProp (node, (xmlChar *)"field", (xmlChar *)filename->data);
	} else {
		xmlSetProp (node, (xmlChar *)"src", (xmlChar *)filename->data);
	}
	gl_text_node_free (&filename);

	/* affine attrs */
	gl_label_object_get_affine (object, affine);
	gl_xml_set_prop_double (node, "a0", affine[0]);
	gl_xml_set_prop_double (node, "a1", affine[1]);
	gl_xml_set_prop_double (node, "a2", affine[2]);
	gl_xml_set_prop_double (node, "a3", affine[3]);
	gl_xml_set_prop_double (node, "a4", affine[4]);
	gl_xml_set_prop_double (node, "a5", affine[5]);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Objects->Object-barcode Node                           */
/*--------------------------------------------------------------------------*/
static void
xml_create_object_barcode (xmlNodePtr     root,
			   xmlNsPtr       ns,
			   glLabelObject *object)
{
	xmlNodePtr        node;
	gdouble           x, y;
	gdouble           w, h;
	glTextNode       *text_node;
	gchar            *id;
	gboolean          text_flag;
	gboolean          checksum_flag;
	glColorNode      *color_node;
	guint             format_digits;
	gdouble           affine[6];

	gl_debug (DEBUG_XML, "START");

	node = xmlNewChild (root, ns, (xmlChar *)"Object-barcode", NULL);

	/* position attrs */
	gl_label_object_get_position (object, &x, &y);
	gl_xml_set_prop_length (node, "x", x);
	gl_xml_set_prop_length (node, "y", y);

	/* size attrs */
	gl_label_object_get_size (object, &w, &h);
	gl_xml_set_prop_length (node, "w", w);
	gl_xml_set_prop_length (node, "h", h);

	/* Barcode properties attrs */
	gl_label_barcode_get_props (GL_LABEL_BARCODE(object),
				    &id, &text_flag, &checksum_flag, &format_digits);
	xmlSetProp (node, (xmlChar *)"style", (xmlChar *)id);
	gl_xml_set_prop_boolean (node, "text", text_flag);
	gl_xml_set_prop_boolean (node, "checksum", checksum_flag);
	
	g_free (id);
	
	color_node = gl_label_object_get_line_color (GL_LABEL_OBJECT(object));
	if (color_node->field_flag)
	{
		xmlSetProp (node, (xmlChar *)"color_field", (xmlChar *)color_node->key);
	}
	else
	{
		gl_xml_set_prop_uint_hex (node, "color", color_node->color);
	}
	gl_color_node_free (&color_node);


	/* data OR field attr */
	text_node = gl_label_barcode_get_data (GL_LABEL_BARCODE(object));
	if (text_node->field_flag) {
		xmlSetProp (node, (xmlChar *)"field", (xmlChar *)text_node->data);
	        gl_xml_set_prop_int (node, "format", format_digits);
	} else {
		xmlSetProp (node, (xmlChar *)"data", (xmlChar *)text_node->data);
	}
	gl_text_node_free (&text_node);

	/* affine attrs */
	gl_label_object_get_affine (object, affine);
	gl_xml_set_prop_double (node, "a0", affine[0]);
	gl_xml_set_prop_double (node, "a1", affine[1]);
	gl_xml_set_prop_double (node, "a2", affine[2]);
	gl_xml_set_prop_double (node, "a3", affine[3]);
	gl_xml_set_prop_double (node, "a4", affine[4]);
	gl_xml_set_prop_double (node, "a5", affine[5]);

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Label Merge Fields Node                                */
/*--------------------------------------------------------------------------*/
static void
xml_create_merge_fields (xmlNodePtr  root,
			 xmlNsPtr    ns,
			 glLabel    *label)
{
	xmlNodePtr  node, child;
	gchar      *string;
	GList      *p;
	glMerge    *merge;

	gl_debug (DEBUG_XML, "START");

	merge = gl_label_get_merge (label);

	node = xmlNewChild (root, ns, (xmlChar *)"Merge", NULL);

	string = gl_merge_get_name (merge);
	xmlSetProp (node, (xmlChar *)"type", (xmlChar *)string);
	g_free (string);

	string = gl_merge_get_src (merge);
	xmlSetProp (node, (xmlChar *)"src", (xmlChar *)string);
	g_free (string);

	g_object_unref (G_OBJECT(merge));

	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Label Data Node                                        */
/*--------------------------------------------------------------------------*/
static void
xml_create_data (xmlNodePtr  root,
		 xmlNsPtr    ns,
		 glLabel    *label)
{
	xmlNodePtr  node;
	GList      *name_list, *p;
	GHashTable *pixbuf_cache;

	gl_debug (DEBUG_XML, "START");

	node = xmlNewChild (root, ns, (xmlChar *)"Data", NULL);

	pixbuf_cache = gl_label_get_pixbuf_cache (label);
	name_list = gl_pixbuf_cache_get_name_list (pixbuf_cache);

	for (p = name_list; p != NULL; p=p->next) {
		xml_create_pixdata (node, ns, label, p->data);
	}

	gl_pixbuf_cache_free_name_list (name_list);


	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Add XML Label Data Pixbuf Node                                 */
/*--------------------------------------------------------------------------*/
static void
xml_create_pixdata (xmlNodePtr  root,
		    xmlNsPtr    ns,
		    glLabel    *label,
		    gchar      *name)
{
	xmlNodePtr  node;
	GHashTable *pixbuf_cache;
	GdkPixbuf  *pixbuf;
	GdkPixdata *pixdata;
	guchar     *stream;
	guint       stream_length;
	gchar      *base64;

	gl_debug (DEBUG_XML, "START");

	pixbuf_cache = gl_label_get_pixbuf_cache (label);

	pixbuf = gl_pixbuf_cache_get_pixbuf (pixbuf_cache, name);
	if ( pixbuf != NULL ) {

		pixdata = g_new0 (GdkPixdata, 1);
		gdk_pixdata_from_pixbuf (pixdata, pixbuf, FALSE);
		stream = gdk_pixdata_serialize (pixdata, &stream_length);
		base64 = gl_base64_encode (stream, stream_length);

		node = xmlNewChild (root, ns, (xmlChar *)"Pixdata", (xmlChar *)base64);
		xmlSetProp (node, (xmlChar *)"name", (xmlChar *)name);
		xmlSetProp (node, (xmlChar *)"encoding", (xmlChar *)"Base64");

		gl_pixbuf_cache_remove_pixbuf (pixbuf_cache, name);

		g_free (pixdata);
		g_free (stream);
		g_free (base64);
	}


	gl_debug (DEBUG_XML, "END");
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Create top-level Span node.                                    */
/*--------------------------------------------------------------------------*/
static void
xml_create_toplevel_span (xmlNodePtr        root,
			  xmlNsPtr          ns,
			  glLabelText      *object_text)
{
	xmlNodePtr        node;
	gchar            *font_family;
	gdouble           font_size;
	GnomeFontWeight   font_weight;
	gboolean          font_italic_flag;
	glColorNode      *color_node;
	GtkJustification  just;
	gdouble           text_line_spacing;
	GList            *lines, *p_line, *p_node;
	glTextNode       *text_node;
	xmlNodePtr        child;

	node = xmlNewChild (root, ns, (xmlChar *)"Span", NULL);

	/* All span attrs at top level. */
	font_family = gl_label_object_get_font_family (GL_LABEL_OBJECT(object_text));
	font_size = gl_label_object_get_font_size (GL_LABEL_OBJECT(object_text));
	text_line_spacing = gl_label_object_get_text_line_spacing (GL_LABEL_OBJECT(object_text));
	font_weight = gl_label_object_get_font_weight (GL_LABEL_OBJECT(object_text));
	font_italic_flag = gl_label_object_get_font_italic_flag (GL_LABEL_OBJECT(object_text));
	
	color_node = gl_label_object_get_text_color (GL_LABEL_OBJECT(object_text));
	if (color_node->field_flag)
	{
		xmlSetProp (node, (xmlChar *)"color_field", (xmlChar *)color_node->key);
	}
	else
	{
		gl_xml_set_prop_uint_hex (node, "color", color_node->color);
	}
	gl_color_node_free (&color_node);
	
	just = gl_label_object_get_text_alignment (GL_LABEL_OBJECT(object_text));
	xmlSetProp (node, (xmlChar *)"font_family", (xmlChar *)font_family);
	gl_xml_set_prop_double (node, "font_size", font_size);
	xmlSetProp (node, (xmlChar *)"font_weight", (xmlChar *)gl_util_weight_to_string (font_weight));
	gl_xml_set_prop_boolean (node, "font_italic", font_italic_flag);
	
	gl_xml_set_prop_double (node, "line_spacing", text_line_spacing);

	/* Build children. */
	lines = gl_label_text_get_lines (GL_LABEL_TEXT(object_text));
	for (p_line = lines; p_line != NULL; p_line = p_line->next) {

		for (p_node = (GList *) p_line->data; p_node != NULL;
		     p_node = p_node->next) {
			text_node = (glTextNode *) p_node->data;

			if (text_node->field_flag) {
				child = xmlNewChild (node, ns, (xmlChar *)"Field", NULL);
				xmlSetProp (child, (xmlChar *)"name", (xmlChar *)text_node->data);
			} else {
				xmlNodeAddContent (node, (xmlChar *)text_node->data);
			}

		}

		if ( p_line->next ) {
			child = xmlNewChild (node, ns, (xmlChar *)"NL", NULL);
		}

	}

	gl_text_node_lines_free (&lines);
	g_free (font_family);

}
