/** 
 * XML Security Library (http://www.aleksey.com/xmlsec).
 *
 * Canonicalization transforms.
 *
 * This is free software; see Copyright file in the source
 * distribution for preciese wording.
 * 
 * Copyright (C) 2002-2003 Aleksey Sanin <aleksey@aleksey.com>
 * Portion Copyright � 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved. 
 */
#include "xmlsec_globals.h" 
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libxml2_tree.h>
#include <libxml2_c14n.h>
#include <libxml2_globals.h>

#include "xmlsec_xmlsec.h"
#include "xmlsec_keys.h"
#include "xmlsec_list.h"
#include "xmlsec_transforms.h"
#include "xmlsec_xmltree.h"
#include "xmlsec_errors.h"


/******************************************************************************
 *
 * C14N transforms
 *
 * Inclusive namespaces list for ExclC14N (xmlSecStringList) is located 
 * after xmlSecTransform structure
 * 
 *****************************************************************************/
#define xmlSecTransformC14NSize	\
    (sizeof(xmlSecTransform) + sizeof(xmlSecPtrList))
#define xmlSecTransformC14NGetNsList(transform) \
    ((xmlSecTransformCheckSize((transform), xmlSecTransformC14NSize)) ? \
	(xmlSecPtrListPtr)(((xmlSecByte*)(transform)) + sizeof(xmlSecTransform)) : \
	(xmlSecPtrListPtr)NULL)

#define xmlSecTransformC14NCheckId(transform) \
    (xmlSecTransformInclC14NCheckId((transform)) || \
     xmlSecTransformExclC14NCheckId((transform)) || \
     xmlSecTransformCheckId((transform), xmlSecTransformRemoveXmlTagsC14NId))
#define xmlSecTransformInclC14NCheckId(transform) \
    (xmlSecTransformCheckId((transform), xmlSecTransformInclC14NId) || \
     xmlSecTransformCheckId((transform), xmlSecTransformInclC14NWithCommentsId))
#define xmlSecTransformExclC14NCheckId(transform) \
    (xmlSecTransformCheckId((transform), xmlSecTransformExclC14NId) || \
     xmlSecTransformCheckId((transform), xmlSecTransformExclC14NWithCommentsId) )


static int		xmlSecTransformC14NInitialize	(xmlSecTransformPtr transform);
static void		xmlSecTransformC14NFinalize	(xmlSecTransformPtr transform);
static int 		xmlSecTransformC14NNodeRead	(xmlSecTransformPtr transform,
							 xmlNodePtr node,
							 xmlSecTransformCtxPtr transformCtx);
static int		xmlSecTransformC14NPushXml	(xmlSecTransformPtr transform, 
							 xmlSecNodeSetPtr nodes,
							 xmlSecTransformCtxPtr transformCtx);
static int		xmlSecTransformC14NPopBin	(xmlSecTransformPtr transform, 
							 xmlSecByte* data,
							 xmlSecSize maxDataSize,
							 xmlSecSize* dataSize,
							 xmlSecTransformCtxPtr transformCtx);
static int		xmlSecTransformC14NExecute	(xmlSecTransformId id, 
							 xmlSecNodeSetPtr nodes, 
							 xmlChar** nsList,
							 xmlOutputBufferPtr buf);
static int
xmlSecTransformC14NInitialize(xmlSecTransformPtr transform) {
    xmlSecPtrListPtr nsList;
    int ret;
    
    xmlSecAssert2(xmlSecTransformC14NCheckId(transform), -1);

    nsList = xmlSecTransformC14NGetNsList(transform);
    xmlSecAssert2(nsList != NULL, -1);
    
    ret = xmlSecPtrListInitialize(nsList, xmlSecStringListId);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
		    "xmlSecPtrListInitialize",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    return(0);
}

static void
xmlSecTransformC14NFinalize(xmlSecTransformPtr transform) {
    xmlSecPtrListPtr nsList;

    xmlSecAssert(xmlSecTransformC14NCheckId(transform));

    nsList = xmlSecTransformC14NGetNsList(transform);
    xmlSecAssert(xmlSecPtrListCheckId(nsList, xmlSecStringListId));
    
    xmlSecPtrListFinalize(nsList);
}

static int
xmlSecTransformC14NNodeRead(xmlSecTransformPtr transform, xmlNodePtr node, xmlSecTransformCtxPtr transformCtx) {
    xmlSecPtrListPtr nsList;
    xmlNodePtr cur;
    xmlChar *list;
    xmlChar *p, *n, *tmp;
    int ret;
    
    /* we have something to read only for exclusive c14n transforms */
    xmlSecAssert2(xmlSecTransformExclC14NCheckId(transform), -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(transformCtx != NULL, -1);
        
    nsList = xmlSecTransformC14NGetNsList(transform);
    xmlSecAssert2(xmlSecPtrListCheckId(nsList, xmlSecStringListId), -1);
    xmlSecAssert2(xmlSecPtrListGetSize(nsList) == 0, -1);
    
    /* there is only one optional node */
    // Venus: Check type to avoid incorrect casting from xmlDocPtr -> xmlNodePtr
    if (node->type == XML_DOCUMENT_NODE)
    	cur = NULL;  
    else
    	cur = xmlSecGetNextElementNode(node->children); 
    if(cur != NULL) {
	if(!xmlSecCheckNodeName(cur, xmlSecNodeInclusiveNamespaces, xmlSecNsExcC14N)) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			xmlSecErrorsSafeString(xmlSecNodeGetName(cur)),
			XMLSEC_ERRORS_R_INVALID_NODE,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
    
        list = xmlGetProp(cur, xmlSecAttrPrefixList);
	if(list == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE, 
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			xmlSecErrorsSafeString(xmlSecAttrPrefixList),
			XMLSEC_ERRORS_R_INVALID_NODE_ATTRIBUTE,
			"node=%s",
			xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
	    return(-1);
	}
    
        /* the list of namespaces is space separated */
	for(p = n = list; ((p != NULL) && ((*p) != '\0')); p = n) {
	    n = (xmlChar*)xmlStrchr(p, ' ');
	    if(n != NULL) {
	        *(n++) = '\0';
	    }	
	
	    tmp = xmlStrdup(p);
	    if(tmp == NULL) {
		xmlSecError(XMLSEC_ERRORS_HERE,
		    	    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			    NULL,
		    	    XMLSEC_ERRORS_R_STRDUP_FAILED,
			    "len=%d", xmlStrlen(p));
		xmlFree(list);
		return(-1);	
	    }
	
	    ret = xmlSecPtrListAdd(nsList, tmp);
	    if(ret < 0) {
		xmlSecError(XMLSEC_ERRORS_HERE,
			    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			    "xmlSecPtrListAdd",
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    XMLSEC_ERRORS_NO_MESSAGE);
	        xmlFree(tmp);
		xmlFree(list);
	        return(-1);
	    }
	}
        xmlFree(list);

	/* add NULL at the end */
        ret = xmlSecPtrListAdd(nsList, NULL);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
		        "xmlSecPtrListAdd",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
		        XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}

	cur = xmlSecGetNextElementNode(cur->next);        
    }
    
    /* check that we have nothing else */
    if(cur != NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    NULL,
		    xmlSecErrorsSafeString(xmlSecNodeGetName(cur)),
		    XMLSEC_ERRORS_R_UNEXPECTED_NODE,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }

    return(0);    
}

static int 
xmlSecTransformC14NPushXml(xmlSecTransformPtr transform, xmlSecNodeSetPtr nodes,
			    xmlSecTransformCtxPtr transformCtx) {
    xmlOutputBufferPtr buf;
    xmlSecPtrListPtr nsList;
    int ret;
    
    xmlSecAssert2(xmlSecTransformC14NCheckId(transform), -1);
    xmlSecAssert2(nodes != NULL, -1);
    xmlSecAssert2(nodes->doc != NULL, -1);
    xmlSecAssert2(transformCtx != NULL, -1);

    /* check/update current transform status */
    switch(transform->status) {
    case xmlSecTransformStatusNone:
	transform->status = xmlSecTransformStatusWorking;
	break;
    case xmlSecTransformStatusWorking:
    case xmlSecTransformStatusFinished:
	return(0);
    default:
	xmlSecError(XMLSEC_ERRORS_HERE, 
		    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
		    NULL,
		    XMLSEC_ERRORS_R_INVALID_STATUS,
		    "status=%d", transform->status);
	return(-1);
    }
    xmlSecAssert2(transform->status == xmlSecTransformStatusWorking, -1);

    /* prepare output buffer: next transform or ourselves */
    if(transform->next != NULL) {
	buf = xmlSecTransformCreateOutputBuffer(transform->next, transformCtx);
	if(buf == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			"xmlSecTransformCreateOutputBuffer",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
    } else {
	buf = xmlSecBufferCreateOutputBuffer(&(transform->outBuf));
	if(buf == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			"xmlSecBufferCreateOutputBuffer",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
    }

    nsList = xmlSecTransformC14NGetNsList(transform);
    xmlSecAssert2(xmlSecPtrListCheckId(nsList, xmlSecStringListId), -1);

    ret = xmlSecTransformC14NExecute(transform->id, nodes, (xmlChar**)(nsList->data), buf);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
		    "xmlSecTransformC14NExecute",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	xmlOutputBufferClose(buf);
	return(-1);
    }
    
    ret = xmlOutputBufferClose(buf);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
		    "xmlOutputBufferClose",
		    XMLSEC_ERRORS_R_XML_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    transform->status = xmlSecTransformStatusFinished;
    return(0);
}

static int 
xmlSecTransformC14NPopBin(xmlSecTransformPtr transform, xmlSecByte* data,
			    xmlSecSize maxDataSize, xmlSecSize* dataSize,
			    xmlSecTransformCtxPtr transformCtx) {
    xmlSecPtrListPtr nsList;
    xmlSecBufferPtr out;
    int ret;
        
    xmlSecAssert2(xmlSecTransformC14NCheckId(transform), -1);
    xmlSecAssert2(data != NULL, -1);
    xmlSecAssert2(dataSize != NULL, -1);
    xmlSecAssert2(transformCtx != NULL, -1);
    
    out = &(transform->outBuf);
    if(transform->status == xmlSecTransformStatusNone) {
	xmlOutputBufferPtr buf;
	
	xmlSecAssert2(transform->inNodes == NULL, -1);
	
	if(transform->prev == NULL) {
	    (*dataSize) = 0;
	    transform->status = xmlSecTransformStatusFinished;
	    return(0);
	}
	
	/* get xml data from previous transform */
	ret = xmlSecTransformPopXml(transform->prev, &(transform->inNodes), transformCtx);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			"xmlSecTransformPopXml",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
		
	/* dump everything to internal buffer */
	buf = xmlSecBufferCreateOutputBuffer(out);
	if(buf == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			"xmlSecBufferCreateOutputBuffer",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
	    
	nsList = xmlSecTransformC14NGetNsList(transform);
	xmlSecAssert2(xmlSecPtrListCheckId(nsList, xmlSecStringListId), -1);

    	ret = xmlSecTransformC14NExecute(transform->id, transform->inNodes, (xmlChar**)(nsList->data), buf);
    	if(ret < 0) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			"xmlSecTransformC14NExecute",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    xmlOutputBufferClose(buf);
	    return(-1);
	}
	ret = xmlOutputBufferClose(buf);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			"xmlOutputBufferClose",
			XMLSEC_ERRORS_R_XML_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    return(-1);
	}
	transform->status = xmlSecTransformStatusWorking;
    }
    
    if(transform->status == xmlSecTransformStatusWorking) {
	xmlSecSize outSize;
	
	/* return chunk after chunk */
	outSize = xmlSecBufferGetSize(out);
	if(outSize > maxDataSize) {	
	    outSize = maxDataSize;
	}
	if(outSize > XMLSEC_TRANSFORM_BINARY_CHUNK) {
	    outSize = XMLSEC_TRANSFORM_BINARY_CHUNK;
	}
	if(outSize > 0) {
	    xmlSecAssert2(xmlSecBufferGetData(&(transform->outBuf)), -1);
	
	    memcpy(data, xmlSecBufferGetData(&(transform->outBuf)), outSize);
	    ret = xmlSecBufferRemoveHead(&(transform->outBuf), outSize);
    	    if(ret < 0) {
		xmlSecError(XMLSEC_ERRORS_HERE,
			    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
			    "xmlSecBufferRemoveHead",
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    "size=%d", outSize);
		return(-1);
	    }	
	} else if(xmlSecBufferGetSize(out) == 0) {
	    transform->status = xmlSecTransformStatusFinished;
	}
	(*dataSize) = outSize;
    } else if(transform->status == xmlSecTransformStatusFinished) {
	/* the only way we can get here is if there is no output */
	xmlSecAssert2(xmlSecBufferGetSize(out) == 0, -1);
	(*dataSize) = 0;
    } else {
	xmlSecError(XMLSEC_ERRORS_HERE, 
		    xmlSecErrorsSafeString(xmlSecTransformGetName(transform)),
		    NULL,
		    XMLSEC_ERRORS_R_INVALID_STATUS,
		    "status=%d", transform->status);
	return(-1);
    }
    
    return(0);
}

static int 
xmlSecTransformC14NExecute(xmlSecTransformId id, xmlSecNodeSetPtr nodes, xmlChar** nsList, 
			   xmlOutputBufferPtr buf) {
    int ret; 
    
    xmlSecAssert2(id != xmlSecTransformIdUnknown, -1);
    xmlSecAssert2(nodes != NULL, -1);
    xmlSecAssert2(nodes->doc != NULL, -1);
    xmlSecAssert2(buf != NULL, -1);

    /* execute c14n transform */
    if(id == xmlSecTransformInclC14NId) {    
    	ret = xmlC14NExecute(nodes->doc, 
			(xmlC14NIsVisibleCallback)xmlSecNodeSetContains, 
			nodes, 0, NULL, 0, buf);
    } else if(id == xmlSecTransformInclC14NWithCommentsId) {
	 ret = xmlC14NExecute(nodes->doc, 
			(xmlC14NIsVisibleCallback)xmlSecNodeSetContains, 
			nodes, 0, NULL, 1, buf); 
    } else if(id == xmlSecTransformExclC14NId) {
	ret = xmlC14NExecute(nodes->doc, 
			(xmlC14NIsVisibleCallback)xmlSecNodeSetContains, 
			nodes, 1, nsList, 0, buf);
    } else if(id == xmlSecTransformExclC14NWithCommentsId) {
	ret = xmlC14NExecute(nodes->doc, 
			(xmlC14NIsVisibleCallback)xmlSecNodeSetContains, 
			nodes, 1, nsList, 1, buf);
    } else if(id == xmlSecTransformRemoveXmlTagsC14NId) { 
    	ret = xmlSecNodeSetDumpTextNodes(nodes, buf);
    } else {
	/* shoudn't be possible to come here, actually */
	xmlSecError(XMLSEC_ERRORS_HERE, 
		    xmlSecErrorsSafeString(xmlSecTransformKlassGetName(id)),
		    NULL,
		    XMLSEC_ERRORS_R_INVALID_TRANSFORM,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE, 
		    xmlSecErrorsSafeString(xmlSecTransformKlassGetName(id)),
		    "xmlC14NExecute",
		    XMLSEC_ERRORS_R_XML_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    return(0);
}

static xmlSecTransformKlass xmlSecTransformInclC14NKlass = {
    /* klass/object sizes */
    sizeof(xmlSecTransformKlass),		/* xmlSecSize klassSize */
    xmlSecTransformC14NSize,			/* xmlSecSize objSize */

    xmlSecNameC14N,				/* const xmlChar* name; */
    xmlSecHrefC14N, 				/* const xmlChar* href; */
    xmlSecTransformUsageC14NMethod | xmlSecTransformUsageDSigTransform,	
						/* xmlSecAlgorithmUsage usage; */

    xmlSecTransformC14NInitialize, 		/* xmlSecTransformInitializeMethod initialize; */
    xmlSecTransformC14NFinalize,		/* xmlSecTransformFinalizeMethod finalize; */
    NULL,					/* xmlSecTransformNodeReadMethod readNode; */
    NULL,					/* xmlSecTransformNodeWriteMethod writeNode; */
    NULL,					/* xmlSecTransformSetKeyReqMethod setKeyReq; */
    NULL,					/* xmlSecTransformSetKeyMethod setKey; */
    NULL,					/* xmlSecTransformValidateMethod validate; */
    xmlSecTransformDefaultGetDataType,		/* xmlSecTransformGetDataTypeMethod getDataType; */
    NULL,					/* xmlSecTransformPushBinMethod pushBin; */
    xmlSecTransformC14NPopBin,			/* xmlSecTransformPopBinMethod popBin; */
    xmlSecTransformC14NPushXml,			/* xmlSecTransformPushXmlMethod pushXml; */
    NULL,					/* xmlSecTransformPopXmlMethod popXml; */
    NULL,					/* xmlSecTransformExecuteMethod execute; */

    NULL,					/* void* reserved0; */
    NULL,					/* void* reserved1; */
};

/**
 * xmlSecTransformInclC14NGetKlass:
 *
 * Inclusive (regular) canonicalization that omits comments transform klass
 * (http://www.w3.org/TR/xmldsig-core/#sec-c14nAlg and 
 * http://www.w3.org/TR/2001/REC-xml-c14n-20010315).
 *
 * Returns c14n transform id.
 */
EXPORT_C
xmlSecTransformId 
xmlSecTransformInclC14NGetKlass(void) {
    return(&xmlSecTransformInclC14NKlass);
}
 
static xmlSecTransformKlass xmlSecTransformInclC14NWithCommentsKlass = {
    /* klass/object sizes */
    sizeof(xmlSecTransformKlass),		/* xmlSecSize klassSize */
    xmlSecTransformC14NSize,			/* xmlSecSize objSize */

    /* same as xmlSecTransformId */    
    xmlSecNameC14NWithComments,			/* const xmlChar* name; */
    xmlSecHrefC14NWithComments, 		/* const xmlChar* href; */
    xmlSecTransformUsageC14NMethod | xmlSecTransformUsageDSigTransform,	
						/* xmlSecAlgorithmUsage usage; */

    xmlSecTransformC14NInitialize, 		/* xmlSecTransformInitializeMethod initialize; */
    xmlSecTransformC14NFinalize,		/* xmlSecTransformFinalizeMethod finalize; */
    NULL,					/* xmlSecTransformNodeReadMethod read; */
    NULL,					/* xmlSecTransformNodeWriteMethod writeNode; */
    NULL,					/* xmlSecTransformSetKeyReqMethod setKeyReq; */
    NULL,					/* xmlSecTransformSetKeyMethod setKey; */
    NULL,					/* xmlSecTransformValidateMethod validate; */
    xmlSecTransformDefaultGetDataType,		/* xmlSecTransformGetDataTypeMethod getDataType; */
    NULL,					/* xmlSecTransformPushBinMethod pushBin; */
    xmlSecTransformC14NPopBin,			/* xmlSecTransformPopBinMethod popBin; */
    xmlSecTransformC14NPushXml,			/* xmlSecTransformPushXmlMethod pushXml; */
    NULL,					/* xmlSecTransformPopXmlMethod popXml; */
    NULL,					/* xmlSecTransformExecuteMethod execute; */

    NULL,					/* void* reserved0; */
    NULL,					/* void* reserved1; */
};

/**
 * xmlSecTransformInclC14NWithCommentsGetKlass:
 *
 * Inclusive (regular) canonicalization that includes comments transform klass
 * (http://www.w3.org/TR/xmldsig-core/#sec-c14nAlg and 
 * http://www.w3.org/TR/2001/REC-xml-c14n-20010315).
 *
 * Returns c14n with comments transform id.
 */
EXPORT_C
xmlSecTransformId 
xmlSecTransformInclC14NWithCommentsGetKlass(void) {
    return(&xmlSecTransformInclC14NWithCommentsKlass);
}

static xmlSecTransformKlass xmlSecTransformExclC14NKlass = {
    /* klass/object sizes */
    sizeof(xmlSecTransformKlass),		/* xmlSecSize klassSize */
    xmlSecTransformC14NSize,			/* xmlSecSize objSize */

    xmlSecNameExcC14N,				/* const xmlChar* name; */
    xmlSecHrefExcC14N,				/* const xmlChar* href; */
    xmlSecTransformUsageC14NMethod | xmlSecTransformUsageDSigTransform,	
						/* xmlSecAlgorithmUsage usage; */

    xmlSecTransformC14NInitialize, 		/* xmlSecTransformInitializeMethod initialize; */
    xmlSecTransformC14NFinalize,		/* xmlSecTransformFinalizeMethod finalize; */
    xmlSecTransformC14NNodeRead,		/* xmlSecTransformNodeReadMethod readNode; */
    NULL,					/* xmlSecTransformNodeWriteMethod writeNode; */
    NULL,					/* xmlSecTransformSetKeyReqMethod setKeyReq; */
    NULL,					/* xmlSecTransformSetKeyMethod setKey; */
    NULL,					/* xmlSecTransformValidateMethod validate; */
    xmlSecTransformDefaultGetDataType,		/* xmlSecTransformGetDataTypeMethod getDataType; */
    NULL,					/* xmlSecTransformPushBinMethod pushBin; */
    xmlSecTransformC14NPopBin,			/* xmlSecTransformPopBinMethod popBin; */
    xmlSecTransformC14NPushXml,			/* xmlSecTransformPushXmlMethod pushXml; */
    NULL,					/* xmlSecTransformPopXmlMethod popXml; */
    NULL,					/* xmlSecTransformExecuteMethod execute; */
    
    NULL,					/* void* reserved0; */
    NULL,					/* void* reserved1; */
};

/** 
 * xmlSecTransformExclC14NGetKlass:
 * 
 * Exclusive canoncicalization that ommits comments transform klass
 * (http://www.w3.org/TR/xml-exc-c14n/).
 * 
 * Returns exclusive c14n transform id.
 */
EXPORT_C
xmlSecTransformId 
xmlSecTransformExclC14NGetKlass(void) {
    return(&xmlSecTransformExclC14NKlass);
}

static xmlSecTransformKlass xmlSecTransformExclC14NWithCommentsKlass = {
    /* klass/object sizes */
    sizeof(xmlSecTransformKlass),		/* xmlSecSize klassSize */
    xmlSecTransformC14NSize,			/* xmlSecSize objSize */

    xmlSecNameExcC14NWithComments,		/* const xmlChar* name; */
    xmlSecHrefExcC14NWithComments,		/* const xmlChar* href; */
    xmlSecTransformUsageC14NMethod | xmlSecTransformUsageDSigTransform,		
						/* xmlSecAlgorithmUsage usage; */

    xmlSecTransformC14NInitialize, 		/* xmlSecTransformInitializeMethod initialize; */
    xmlSecTransformC14NFinalize,		/* xmlSecTransformFinalizeMethod finalize; */
    xmlSecTransformC14NNodeRead,		/* xmlSecTransformNodeReadMethod readNode; */
    NULL,					/* xmlSecTransformNodeWriteMethod writeNode; */
    NULL,					/* xmlSecTransformSetKeyReqMethod setKeyReq; */
    NULL,					/* xmlSecTransformSetKeyMethod setKey; */
    NULL,					/* xmlSecTransformValidateMethod validate; */
    xmlSecTransformDefaultGetDataType,		/* xmlSecTransformGetDataTypeMethod getDataType; */
    NULL,					/* xmlSecTransformPushBinMethod pushBin; */
    xmlSecTransformC14NPopBin,			/* xmlSecTransformPopBinMethod popBin; */
    xmlSecTransformC14NPushXml,			/* xmlSecTransformPushXmlMethod pushXml; */
    NULL,					/* xmlSecTransformPopXmlMethod popXml; */
    NULL,					/* xmlSecTransformExecuteMethod execute; */

    NULL,					/* void* reserved0; */
    NULL,					/* void* reserved1; */
};

/** 
 * xmlSecTransformExclC14NWithCommentsGetKlass:
 * 
 * Exclusive canoncicalization that includes comments transform klass
 * (http://www.w3.org/TR/xml-exc-c14n/).
 * 
 * Returns exclusive c14n with comments transform id.
 */
EXPORT_C
xmlSecTransformId 
xmlSecTransformExclC14NWithCommentsGetKlass(void) {
    return(&xmlSecTransformExclC14NWithCommentsKlass);
}

static xmlSecTransformKlass xmlSecTransformRemoveXmlTagsC14NKlass = {
    /* klass/object sizes */
    sizeof(xmlSecTransformKlass),		/* xmlSecSize klassSize */
    xmlSecTransformC14NSize,			/* xmlSecSize objSize */

    BAD_CAST "remove-xml-tags-transform",	/* const xmlChar* name; */
    NULL, 					/* const xmlChar* href; */
    xmlSecTransformUsageC14NMethod | xmlSecTransformUsageDSigTransform,		
						/* xmlSecAlgorithmUsage usage; */

    xmlSecTransformC14NInitialize, 		/* xmlSecTransformInitializeMethod initialize; */
    xmlSecTransformC14NFinalize,		/* xmlSecTransformFinalizeMethod finalize; */
    NULL,					/* xmlSecTransformNodeReadMethod readNode; */
    NULL,					/* xmlSecTransformNodeWriteMethod writeNode; */
    NULL,					/* xmlSecTransformSetKeyReqMethod setKeyReq; */
    NULL,					/* xmlSecTransformSetKeyMethod setKey; */
    NULL,					/* xmlSecTransformValidateMethod validate; */
    xmlSecTransformDefaultGetDataType,		/* xmlSecTransformGetDataTypeMethod getDataType; */
    NULL,					/* xmlSecTransformPushBinMethod pushBin; */
    xmlSecTransformC14NPopBin,			/* xmlSecTransformPopBinMethod popBin; */
    xmlSecTransformC14NPushXml,			/* xmlSecTransformPushXmlMethod pushXml; */
    NULL,					/* xmlSecTransformPopXmlMethod popXml; */
    NULL,					/* xmlSecTransformExecuteMethod execute; */

    NULL,					/* void* reserved0; */
    NULL,					/* void* reserved1; */
};

/**
 * xmlSecTransformRemoveXmlTagsC14NGetKlass:
 *
 * The "remove xml tags" transform klass (http://www.w3.org/TR/xmldsig-core/#sec-Base-64):
 * Base64 transform requires an octet stream for input. If an XPath node-set 
 * (or sufficiently functional alternative) is given as input, then it is 
 * converted to an octet stream by performing operations logically equivalent 
 * to 1) applying an XPath transform with expression self::text(), then 2) 
 * taking the string-value of the node-set. Thus, if an XML element is 
 * identified by a barename XPointer in the Reference URI, and its content 
 * consists solely of base64 encoded character data, then this transform 
 * automatically strips away the start and end tags of the identified element 
 * and any of its descendant elements as well as any descendant comments and 
 * processing instructions. The output of this transform is an octet stream.
 *
 * Returns "remove xml tags" transform id.
 */
EXPORT_C
xmlSecTransformId 
xmlSecTransformRemoveXmlTagsC14NGetKlass(void) {
    return(&xmlSecTransformRemoveXmlTagsC14NKlass);
}



