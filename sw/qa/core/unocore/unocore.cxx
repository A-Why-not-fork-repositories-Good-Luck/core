/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <swmodeltestbase.hxx>

#include <com/sun/star/awt/FontSlant.hpp>
#include <com/sun/star/table/XCellRange.hpp>
#include <com/sun/star/text/TextContentAnchorType.hpp>
#include <com/sun/star/text/AutoTextContainer.hpp>
#include <com/sun/star/text/VertOrientation.hpp>
#include <com/sun/star/text/XAutoTextGroup.hpp>
#include <com/sun/star/text/XTextFrame.hpp>
#include <com/sun/star/text/XTextPortionAppend.hpp>
#include <com/sun/star/text/XTextContentAppend.hpp>
#include <com/sun/star/text/XTextRangeCompare.hpp>
#include <com/sun/star/text/XTextAppend.hpp>
#include <com/sun/star/rdf/URI.hpp>
#include <com/sun/star/rdf/URIs.hpp>
#include <com/sun/star/awt/XDevice.hpp>
#include <com/sun/star/awt/XToolkit.hpp>
#include <com/sun/star/graphic/XGraphic.hpp>
#include <com/sun/star/style/LineSpacing.hpp>
#include <com/sun/star/view/XSelectionSupplier.hpp>
#include <com/sun/star/text/XTextViewCursorSupplier.hpp>
#include <com/sun/star/text/XDependentTextField.hpp>

#include <comphelper/propertyvalue.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <vcl/graphicfilter.hxx>
#include <vcl/errinf.hxx>

#include <wrtsh.hxx>
#include <ndtxt.hxx>
#include <swdtflvr.hxx>
#include <view.hxx>
#include <PostItMgr.hxx>
#include <postithelper.hxx>
#include <AnnotationWin.hxx>
#include <flyfrm.hxx>
#include <fmtanchr.hxx>
#include <unotextrange.hxx>
#include <view.hxx>

using namespace ::com::sun::star;

namespace
{
char const DATA_DIRECTORY[] = "/sw/qa/core/unocore/data/";
}

/// Covers sw/source/core/unocore/ fixes.
class SwCoreUnocoreTest : public SwModelTestBase
{
};

CPPUNIT_TEST_FIXTURE(SwCoreUnocoreTest, testTdf119081)
{
    // Load a doc with a nested table in it.
    load(DATA_DIRECTORY, "tdf119081.odt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    CPPUNIT_ASSERT(pTextDoc);
    SwDocShell* pDocShell = pTextDoc->GetDocShell();
    SwWrtShell* pWrtShell = pDocShell->GetWrtShell();

    // Enter outer A1.
    pWrtShell->Down(/*bSelect=*/false, /*nCount=*/3);
    // Enter inner A1.
    pWrtShell->Right(CRSR_SKIP_CELLS, /*bSelect=*/false, /*nCount=*/1, /*bBasicCall=*/false,
                     /*bVisual=*/true);
    // Enter outer B1.
    pWrtShell->Down(/*bSelect=*/false, /*nCount=*/2);

    SwDoc* pDoc = pDocShell->GetDoc();
    SwPaM& rCursor = pWrtShell->GetCurrentShellCursor();
    uno::Reference<text::XTextRange> xInsertPosition
        = SwXTextRange::CreateXTextRange(*pDoc, *rCursor.GetPoint(), nullptr);
    uno::Reference<text::XTextAppend> xTextAppend(xInsertPosition->getText(), uno::UNO_QUERY);
    // Without the accompanying fix in place, this test would have failed with:
    // An uncaught exception of type com.sun.star.uno.RuntimeException
    xTextAppend->insertTextPortion("x", {}, xInsertPosition);

    // Verify that the string is indeed inserted.
    pWrtShell->Left(CRSR_SKIP_CELLS, /*bSelect=*/true, /*nCount=*/1, /*bBasicCall=*/false,
                    /*bVisual=*/true);
    CPPUNIT_ASSERT_EQUAL(OUString("x"), pWrtShell->GetCurrentShellCursor().GetText());
}

CPPUNIT_TEST_FIXTURE(SwCoreUnocoreTest, flyAtParaAnchor)
{
    mxComponent = loadFromDesktop("private:factory/swriter", "com.sun.star.text.TextDocument");
    uno::Reference<lang::XMultiServiceFactory> const xMSF(mxComponent, uno::UNO_QUERY_THROW);
    uno::Reference<text::XTextDocument> const xTD(mxComponent, uno::UNO_QUERY_THROW);
    uno::Reference<text::XTextFrame> const xTextFrame(
        xMSF->createInstance("com.sun.star.text.TextFrame"), uno::UNO_QUERY_THROW);
    uno::Reference<beans::XPropertySet> const xFrameProps(xTextFrame, uno::UNO_QUERY_THROW);
    xFrameProps->setPropertyValue("AnchorType",
                                  uno::makeAny(text::TextContentAnchorType_AT_PARAGRAPH));
    auto const xText = xTD->getText();
    auto const xTextCursor = xText->createTextCursor();
    CPPUNIT_ASSERT(xTextCursor.is());
    xText->insertTextContent(xTextCursor, xTextFrame, false);
    auto const xAnchor = xTextFrame->getAnchor();
    uno::Reference<text::XTextContent> const xFieldmark(
        xMSF->createInstance("com.sun.star.text.Fieldmark"), uno::UNO_QUERY_THROW);
    // this crashed because the anchor didn't have SwIndex
    xText->insertTextContent(xAnchor, xFieldmark, false);
}

CPPUNIT_TEST_FIXTURE(SwCoreUnocoreTest, testViewCursorTextFrame)
{
    // Given a document with a graphic and holding a reference to that graphic frame:
    mxComponent = loadFromDesktop("private:factory/swriter", "com.sun.star.text.TextDocument");
    uno::Sequence<beans::PropertyValue> aInsertArgs = { comphelper::makePropertyValue(
        "FileName", m_directories.getURLFromSrc(DATA_DIRECTORY) + "graphic.png") };
    dispatchCommand(mxComponent, ".uno:InsertGraphic", aInsertArgs);
    uno::Reference<frame::XModel> xModel(mxComponent, uno::UNO_QUERY);
    uno::Reference<text::XTextViewCursorSupplier> xTextViewCursorSupplier(
        xModel->getCurrentController(), uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xViewCursor(xTextViewCursorSupplier->getViewCursor(),
                                                    uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xFrame;
    xViewCursor->getPropertyValue("TextFrame") >>= xFrame;

    // When saving to ODT, then make sure the store doesn't fail:
    uno::Reference<frame::XStorable> xStorable(xModel, uno::UNO_QUERY);
    uno::Sequence<beans::PropertyValue> aStoreArgs
        = { comphelper::makePropertyValue("FilterName", OUString("writer8")) };
    // Without the accompanying fix in place, this test would have failed with:
    // uno.RuntimeException: "SwXParagraph: disposed or invalid ..."
    xStorable->storeToURL(maTempFile.GetURL(), aStoreArgs);
}

/// Fails the test if an error popup would be presented.
static void BasicDisplayErrorHandler(const OUString& /*rErr*/, const OUString& /*rAction*/)
{
    CPPUNIT_ASSERT(false);
}

CPPUNIT_TEST_FIXTURE(SwCoreUnocoreTest, testBrokenEmbeddedObject)
{
    // Given a document with a broken embedded object (the XML markup is not well-formed):
    load(DATA_DIRECTORY, "broken-embedded-object.odt");
    uno::Reference<text::XTextEmbeddedObjectsSupplier> xSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xObjects(xSupplier->getEmbeddedObjects(),
                                                     uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xObject(xObjects->getByIndex(0), uno::UNO_QUERY);
    uno::Reference<lang::XServiceInfo> xEmbeddedObject;
    // Get the property first, which initializes Draw, which would overwrite our error handler.
    xObject->getPropertyValue("EmbeddedObject") >>= xEmbeddedObject;
    ErrorRegistry::RegisterDisplay(&BasicDisplayErrorHandler);

    // When trying to load that embedded object:
    xObject->getPropertyValue("EmbeddedObject") >>= xEmbeddedObject;

    // Then make sure we get a non-empty reference and an error popup it not shown:
    CPPUNIT_ASSERT(xEmbeddedObject.is());
    // Without the accompanying fix in place, we got this reference, but first an error popup was
    // shown to the user.
    CPPUNIT_ASSERT(
        xEmbeddedObject->supportsService("com.sun.star.comp.embed.OCommonEmbeddedObject"));
}

CPPUNIT_TEST_FIXTURE(SwModelTestBase, testUserFieldTooltip)
{
    // Given a document with a user field:
    loadURL("private:factory/swriter", nullptr);
    uno::Reference<lang::XMultiServiceFactory> xFactory(mxComponent, uno::UNO_QUERY);
    uno::Reference<text::XDependentTextField> xField(
        xFactory->createInstance("com.sun.star.text.TextField.User"), uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xMaster(
        xFactory->createInstance("com.sun.star.text.FieldMaster.User"), uno::UNO_QUERY);
    xMaster->setPropertyValue("Name", uno::makeAny(OUString("a_user_field")));
    xField->attachTextFieldMaster(xMaster);
    xField->getTextFieldMaster()->setPropertyValue("Content", uno::makeAny(OUString("42")));
    uno::Reference<text::XTextDocument> xDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<text::XText> xText = xDocument->getText();
    xText->insertTextContent(xText->createTextCursor(), xField, /*bAbsorb=*/false);
    uno::Reference<beans::XPropertySet> xFieldProps(xField, uno::UNO_QUERY);

    // When setting a tooltip on the field:
    OUString aExpected("first line\nsecond line");
    xFieldProps->setPropertyValue("Title", uno::makeAny(aExpected));

    // Then make sure that the tooltip we read back matches the one previously specified:
    // Without the accompanying fix in place, this test would have failed with:
    // - the property is of unexpected type or void: Title
    // i.e. reading of the tooltip was broken.
    CPPUNIT_ASSERT_EQUAL(aExpected, getProperty<OUString>(xFieldProps, "Title"));
}

CPPUNIT_TEST_FIXTURE(SwModelTestBase, testImageTooltip)
{
    // Given a document with an image and a hyperlink on it:
    loadURL("private:factory/swriter", nullptr);
    uno::Reference<lang::XMultiServiceFactory> xFactory(mxComponent, uno::UNO_QUERY);
    uno::Reference<text::XTextDocument> xDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<text::XText> xText = xDocument->getText();
    uno::Reference<text::XTextCursor> xCursor = xText->createTextCursor();
    uno::Reference<text::XTextContent> xImage(
        xFactory->createInstance("com.sun.star.text.TextGraphicObject"), uno::UNO_QUERY);
    xText->insertTextContent(xCursor, xImage, /*bAbsorb=*/false);
    uno::Reference<beans::XPropertySet> xImageProps(xImage, uno::UNO_QUERY);
    xImageProps->setPropertyValue("HyperLinkURL", uno::makeAny(OUString("http://www.example.com")));

    // When setting a tooltip on the image:
    OUString aExpected("first line\nsecond line");
    xImageProps->setPropertyValue("Tooltip", uno::makeAny(aExpected));

    // Then make sure that the tooltip we read back matches the one previously specified:
    // Without the accompanying fix in place, this test would have failed with:
    // An uncaught exception of type com.sun.star.beans.UnknownPropertyException
    // i.e. reading/writing of the tooltip was broken.
    CPPUNIT_ASSERT_EQUAL(aExpected, getProperty<OUString>(xImageProps, "Tooltip"));
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
