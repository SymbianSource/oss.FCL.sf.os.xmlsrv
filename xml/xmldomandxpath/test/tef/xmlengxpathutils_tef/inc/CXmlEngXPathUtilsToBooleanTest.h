// Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
//

/**
 @file CXmlEngXPathUtilsToBooleanTest.h
 @internalTechnology
*/
#ifndef CXMLENGXPATHUTILSTOBOOLEANTEST_H_
#define CXMLENGXPATHUTILSTOBOOLEANTEST_H_

#include <test/testexecutestepbase.h>

//Names of the sub steps of the test case
_LIT(KXmlEngXPathUtilsToBooleanTest, "XmlEngXPathUtilsToBoolean");


class CXmlEngXPathUtilsToBooleanTest : public CTestStep
{
public:
	//The constructor is passed the step name 
	CXmlEngXPathUtilsToBooleanTest(const TDesC& aStepName);

	//This method is impelemented and from here various sub steps of the tests are invoked
	virtual TVerdict doTestStepL(void);
	
	//These are the methods that are the sub steps for the testing
	TVerdict TestKXmlEngXPathUtilsToBoolean();
};

#endif /*CXMLENGXPATHUTILSTOBOOLEANTEST_H_*/
