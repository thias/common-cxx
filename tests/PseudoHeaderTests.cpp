#include "PseudoHeaderTests.hpp"
#include "../pseudoheader.h"
#include "../fiftyone.h"

#define PSEUDO_BUFFER_SIZE 100
#define PSEUDO_HEADERS_SIZE 3
#define HEADERS_SIZE 8
#define HEADERS_NO_PSEUDO_SIZE 2
#define PREFIXES_SIZE 2

// Order of precedence for accepted prefixes
static const EvidencePrefix orderOfPrecedence[PREFIXES_SIZE] =
	{
		FIFTYONE_DEGREES_EVIDENCE_QUERY,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
	};

// Fixed set of unique headers
static const char *uniqueHeaders[HEADERS_SIZE] = {
	"header1",
	"header2",
	"header3",
	"header4",
	"header5",
	"header1\x1Fheader2",
	"header2\x1Fheader3",
	"header1\x1Fheader2\x1Fheader3"
};

static const char *uniqueHeadersNoPseudoHeader[HEADERS_NO_PSEUDO_SIZE] = {
	"header1",
	"header2"
};

void PseudoHeaderTests::SetUp() {
	Base::SetUp();
	evidence = EvidenceCreate(HEADERS_SIZE);
	evidence->pseudoEvidence = EvidenceCreate(PSEUDO_HEADERS_SIZE);
	for (int i = 0; i < PSEUDO_HEADERS_SIZE; i++) {
		evidence->pseudoEvidence->items[i].originalValue =
			Malloc(PSEUDO_BUFFER_SIZE);
		EXPECT_TRUE(evidence->pseudoEvidence->items[i].originalValue != NULL);
		memset(
			(void*)evidence->pseudoEvidence->items[i].originalValue,
			'\0',
			PSEUDO_BUFFER_SIZE);
	}
}

void PseudoHeaderTests::TearDown() {
	for (int i = 0; i < PSEUDO_HEADERS_SIZE; i++) {
		fiftyoneDegreesFree(
			(void *)evidence->pseudoEvidence->items[i].originalValue);
	}
	HeadersFree(acceptedHeaders);
	EvidenceFree(evidence->pseudoEvidence);
	EvidenceFree(evidence);
	if (strings != nullptr) {
		delete strings;
	}
	Base::TearDown();
}

/*
 * Create an headers structure with the specified capacity and a string
 * collection constructed by the StringCollection class from the list of
 * header names provided, using the create method in headers.c. The
 * expected memory allocation is calculated, and the actual memory
 * allocation is tracked. The structure is freed automatically after each
 * test, at which point the expected and actual memory allocation is
 * checked for equality.
 */
void PseudoHeaderTests::createHeaders(
	const char** headersList,
	int headersCount,
	bool expectUpperPrefixedHeaders) {
	int count = headersCount;
	strings = new StringCollection(headersList, count);
	acceptedHeaders = HeadersCreate(
		expectUpperPrefixedHeaders,
		strings->getState(),
		getHeaderUniqueId);
}

void PseudoHeaderTests::addEvidence(
	testKeyValuePair* evidenceList,
	int size,
	EvidencePrefix prefix) {
	for (int i = 0; i < size; i++) {
		EvidenceAddString(
			evidence,
			prefix,
			evidenceList[i].key,
			evidenceList[i].value
		);
	}
}

void PseudoHeaderTests::checkResults(
	const testExpectedResult *expectedEvidence,
	int size) {
	EXPECT_EQ(size, evidence->pseudoEvidence->count) <<
		"Incorrect number of pseudo evidence constructed, here it should be" <<
		size << "\n";
	for (int i = 0; i < size; i++) {
		EXPECT_EQ(
			expectedEvidence[i].prefix,
			evidence->pseudoEvidence->items[i].prefix) <<
			"Prefix created should be " << expectedEvidence[i].prefix << "\n";
		EXPECT_EQ(0, strcmp(
			expectedEvidence[i].result,
			(const char*)evidence->pseudoEvidence->items[i].originalValue)) <<
			"Pseudo Evidence is not the same where  it should be " <<
			expectedEvidence[i].result << "\n";
	}
}

void PseudoHeaderTests::removePseudoEvidence(size_t bufferSize) {
	// Test if free work correctly
	PseudoHeadersRemoveEvidence(evidence, bufferSize);
	EXPECT_EQ(0, evidence->pseudoEvidence->count = 0);
	for (uint32_t i = 0; i < evidence->pseudoEvidence->capacity; i++) {
		EXPECT_EQ(NULL, evidence->pseudoEvidence->items[i].field) <<
			"Field should be set to NULL\n";
		EXPECT_EQ('\0',
			((const char*)
				evidence->pseudoEvidence->items[i].originalValue)[0]) <<
			"Memory should be reset to all NULL\n";
	}
}

/*
 * Check that pseudo evidence are created correctly if their corresponding
 * request evidence are present in the evidence collection.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationPositiveValidInput) {
	// Expected value
	const testExpectedResult expectedEvidence[3] =
	{
		{
			"value1\x1Fvalue2",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		},
		{
			"value2\x1Fvalue3",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		},
		{
			"value1\x1Fvalue2\x1Fvalue3",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		}
	};

	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair evidenceList[3] =
	{ {"header3", "value3"}, {"header1", "value1"}, {"header2", "value2"} };
	addEvidence(evidenceList, 3, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	checkResults(expectedEvidence, 3);
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that pseudo evidence won't be created if only part of the request
 * evidence present in the evidence collection. 
 */
TEST_F(PseudoHeaderTests, EvidenceCreationPositivePartialInput) {
	// Expected value
	const testExpectedResult expectedEvidence[1] =
	{
		{
			"value2\x1Fvalue3",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		}
	};

	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair evidenceList[2] =
	{ {"header3", "value3"}, {"header2", "value2"}};
	addEvidence(evidenceList, 2, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	checkResults(expectedEvidence, 1);
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that if the request evidence with other prefixes than 'header' or
 * 'query' will not be considered when constructing the pseudo evidence.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationPositiveCookieInput) {
	// Expected value
	const testExpectedResult expectedEvidence[2] =
	{
		{
			"value1\x1Fvalue2",
			FIFTYONE_DEGREES_EVIDENCE_QUERY
		},
		{
			"value2\x1Fvalue3",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		}
	};

	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair headerEvidenceList[2] =
	{ {"header3", "value3"}, {"header2", "value2"} };
	addEvidence(
		headerEvidenceList,
		2,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	testKeyValuePair queryEvidenceList[2] =
	{ {"header1", "value1"}, {"header2", "value2"} };
	addEvidence(
		queryEvidenceList,
		2,
		FIFTYONE_DEGREES_EVIDENCE_QUERY);

	testKeyValuePair queryEvidence = { "header1", "value1" };
	addEvidence(&queryEvidence, 1, FIFTYONE_DEGREES_EVIDENCE_COOKIE);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	checkResults(expectedEvidence, 2);
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that if the request evidence contains a two complete set of evidence
 * for the same pseudo header, the set with higher order of precedence should
 * be picked.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationOrderOfPrecedenceDuplicate) {
	// Expected value
	const testExpectedResult expectedEvidence[1] =
	{
		{
			"value1\x1Fvalue2",
			FIFTYONE_DEGREES_EVIDENCE_QUERY
		}
	};

	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair headerEvidenceList[2] =
	{ {"header1", "value1"}, {"header2", "value2"} };
	addEvidence(
		headerEvidenceList,
		2,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	testKeyValuePair queryEvidenceList[2] =
	{ {"header1", "value1"}, {"header2", "value2"} };
	addEvidence(
		queryEvidenceList,
		2,
		FIFTYONE_DEGREES_EVIDENCE_QUERY);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	checkResults(expectedEvidence, 1);
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that if the request evidence contains a two set of evidence for the
 * same pseudo header, only the complete set will be picked.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationOrderOfPrecedenceNoDuplicate) {
	// Expected value
	const testExpectedResult expectedEvidence[1] =
	{
		{
			"value1\x1Fvalue2",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		}
	};

	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair headerEvidenceList[2] =
	{ {"header1", "value1"}, {"header2", "value2"} };
	addEvidence(
		headerEvidenceList,
		2,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	testKeyValuePair queryEvidenceList[1] =
	{ {"header2", "value2"} };
	addEvidence(
		queryEvidenceList,
		1,
		FIFTYONE_DEGREES_EVIDENCE_QUERY);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	checkResults(expectedEvidence, 1);
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that if no pseuo header are present, no pseuo evidence will be created
 */
TEST_F(PseudoHeaderTests, EvidenceCreationNoPseudoHeaders) {
	// Create headers
	createHeaders(uniqueHeadersNoPseudoHeader, HEADERS_NO_PSEUDO_SIZE, false);

	testKeyValuePair evidenceList[3] =
	{ {"header1", "value1"}, {"header2", "value2"}, {"header3", "value3"} };
	addEvidence(evidenceList, 3, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	EXPECT_EQ(0, evidence->pseudoEvidence->count) <<
		"No pseudo evidence should has been added\n";
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that if no request evidence are present, no pseudo evidence will be
 * created.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationNoRequestHeadersForPseudoHeaders) {
	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair evidenceList[2] =
	{ {"header4", "value4"}, {"header5", "value5"} };
	addEvidence(evidenceList, 2, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	EXPECT_EQ(0, evidence->pseudoEvidence->count) <<
		"No pseudo evidence should has been added\n";
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that if the request evidence are present with blank values, no pseudo
 * evidence will be created.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationBlankRequestHeadersForPseudoHeaders) {
	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair evidenceList[2] =
	{ {"header1", ""}, {"header2", ""} };
	addEvidence(evidenceList, 2, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	EXPECT_EQ(0, evidence->pseudoEvidence->count) <<
		"No pseudo evidence should has been added\n";
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that the PseudoHeadersAddEvidence APIs handle NULL pointer input
 * correctly.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationNullPointerInput) {
	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	// Check if exception is set correctly for NULL evidence pointer
	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		NULL,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NULL_POINTER, exception->status) <<
		"Status code should be NULL_POINTER where evidence pointer is null\n";

	// Check if exception is set correctly for NULL headers pointer
	PseudoHeadersAddEvidence(
		evidence,
		NULL,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NULL_POINTER, exception->status) <<
		"Status code should be NULL_POINTER where headers pointer is null\n";
}

/*
 * Check that pseudo evidence that has already been provided in the evidence
 * collection will not be created again.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationPseudoEvidenceAlreadyIncluded) {
	// Expected value
	const testExpectedResult expectedEvidence[2] =
	{
		{
			"value1\x1Fvalue2",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		},
		{
			"value1\x1Fvalue2\x1Fvalue3",
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING
		}
	};

	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair evidenceList[3] =
	{ 
		{"header1", "value1"}, 
		{"header2", "value2"}, 
		{"header3", "value3"}
	};
	addEvidence(evidenceList, 3, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	testKeyValuePair queryEvidence =
		{ "header2\x1Fheader3", "value2\x1Fvalue3" };
	addEvidence(&queryEvidence, 1, FIFTYONE_DEGREES_EVIDENCE_QUERY);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	checkResults(expectedEvidence, 2);
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that no pseudo evidence is created if all already been provied in the
 * evidence collection.
 */
TEST_F(PseudoHeaderTests, EvidenceCreationPseudoEvidenceAllAlreadyIncluded) {
	// Create headers
	createHeaders(uniqueHeaders, HEADERS_SIZE, false);

	testKeyValuePair evidenceList[3] =
	{
		{"header1", "value1"},
		{"header2", "value2"},
		{"header3", "value3"}
	};
	addEvidence(evidenceList, 3, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);

	testKeyValuePair evidenceList2[3] =
	{
		{"header1\x1Fheader2", "value1\x1Fvalue2"},
		{ "header2\x1Fheader3", "value2\x1Fvalue3" },
		{ "header1\x1Fheader2\x1Fheader3", "value1\x1Fvalue2\x1Fvalue3" }
	};
	addEvidence(evidenceList2, 3, FIFTYONE_DEGREES_EVIDENCE_QUERY);

	EXCEPTION_CREATE;
	PseudoHeadersAddEvidence(
		evidence,
		acceptedHeaders,
		PSEUDO_BUFFER_SIZE,
		orderOfPrecedence,
		PREFIXES_SIZE,
		exception);
	EXCEPTION_THROW;

	EXPECT_EQ(0, evidence->pseudoEvidence->count) <<
		"No new evidence should have been added\n";
	removePseudoEvidence(PSEUDO_BUFFER_SIZE);
}

/*
 * Check that PseudoHeadersRemoveEvidence API deal with NULL pointer input
 * correctly.
 */
TEST_F(PseudoHeaderTests, EvidenceRemoveNullPointerInput) {
	// Should not crash if evidence input is NULL
	PseudoHeadersRemoveEvidence(NULL, 0);

	// Should not crash if evidence how a NULL pointer for pseudo evidence
	EvidenceKeyValuePairArray* savePseudoEvidence =
		evidence->pseudoEvidence;
	evidence->pseudoEvidence = NULL;
	PseudoHeadersRemoveEvidence(evidence, PSEUDO_BUFFER_SIZE);
	evidence->pseudoEvidence = savePseudoEvidence;
}