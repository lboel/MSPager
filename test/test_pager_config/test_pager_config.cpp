#include <gtest/gtest.h>

#include <PagerConfig.h>
#include <string.h>

static bool parse(const char* yaml, PagerConfig& cfg, PagerCfgError& err) {
  return pagerCfgParseYaml(yaml, strlen(yaml), cfg, err);
}

static const char* FULL =
  "# MSPager setup\n"
  "version: 1\n"
  "nickname: Anna\n"
  "channel:\n"
  "  name: Familie   # group channel\n"
  "  key: 8b3387e9c5cdea6ac9e5edbaa115cd72\n"
  "questions:\n"
  "  - text: Wann kommst du?\n"
  "    replies: [\"5 min\", '30 min', Sp\xC3\xA4ter, \"\\U0001F4DE\"]\n"
  "  - text: \"Essen fertig # jetzt\"\n"
  "    replies:\n"
  "      - Komme\n"
  "      - 'Geht''s auch sp\xC3\xA4ter?'\n";

TEST(PagerConfig, ParsesFullDocument) {
  PagerConfig cfg;
  PagerCfgError err;
  ASSERT_TRUE(parse(FULL, cfg, err)) << err.line << ": " << err.msg;

  EXPECT_EQ(PAGER_CFG_HAS_NICKNAME | PAGER_CFG_HAS_CHANNEL | PAGER_CFG_HAS_KEY, cfg.flags);
  EXPECT_STREQ("Anna", cfg.nickname);
  EXPECT_STREQ("Familie", cfg.channel_name);
  EXPECT_EQ(0x8b, cfg.channel_key[0]);
  EXPECT_EQ(0x72, cfg.channel_key[15]);

  ASSERT_EQ(2, cfg.num_questions);
  EXPECT_STREQ("Wann kommst du?", cfg.questions[0].text);
  ASSERT_EQ(4, cfg.questions[0].num_replies);
  EXPECT_STREQ("5 min", cfg.questions[0].replies[0]);
  EXPECT_STREQ("30 min", cfg.questions[0].replies[1]);
  EXPECT_STREQ("Sp\xC3\xA4ter", cfg.questions[0].replies[2]);
  EXPECT_STREQ("\xF0\x9F\x93\x9E", cfg.questions[0].replies[3]);

  EXPECT_STREQ("Essen fertig # jetzt", cfg.questions[1].text);
  ASSERT_EQ(2, cfg.questions[1].num_replies);
  EXPECT_STREQ("Komme", cfg.questions[1].replies[0]);
  EXPECT_STREQ("Geht's auch sp\xC3\xA4ter?", cfg.questions[1].replies[1]);
}

// PyYAML default output: sequences at the parent's indent, sorted keys, escaped non-ASCII
TEST(PagerConfig, ParsesPyYamlStyle) {
  const char* yaml =
    "channel:\r\n"
    "  key: 00112233445566778899aabbccddeeff\r\n"
    "questions:\r\n"
    "- replies:\r\n"
    "  - Ja\r\n"
    "  - \"Sp\\xE4ter\"\r\n"
    "  text: Alles gut?\r\n"
    "- replies: [OK]\r\n"
    "  text: 'No'\r\n";
  PagerConfig cfg;
  PagerCfgError err;
  ASSERT_TRUE(parse(yaml, cfg, err)) << err.line << ": " << err.msg;
  EXPECT_EQ(PAGER_CFG_HAS_KEY, cfg.flags);
  ASSERT_EQ(2, cfg.num_questions);
  EXPECT_STREQ("Alles gut?", cfg.questions[0].text);
  EXPECT_STREQ("Sp\xC3\xA4ter", cfg.questions[0].replies[1]);
  EXPECT_STREQ("No", cfg.questions[1].text);
  EXPECT_STREQ("OK", cfg.questions[1].replies[0]);
}

// PyYAML < 5.1 / default_flow_style=None: flow mapping for channel, flow lists
TEST(PagerConfig, ParsesFlowChannel) {
  const char* yaml =
    "channel: {key: 00112233445566778899aabbccddeeff, name: 'Fam, ily'}\n"
    "questions:\n"
    "- replies: [5 min, Sp\xC3\xA4ter]\n"
    "  text: Wann kommst du?\n";
  PagerConfig cfg;
  PagerCfgError err;
  ASSERT_TRUE(parse(yaml, cfg, err)) << err.line << ": " << err.msg;
  EXPECT_EQ(PAGER_CFG_HAS_KEY | PAGER_CFG_HAS_CHANNEL, cfg.flags);
  EXPECT_STREQ("Fam, ily", cfg.channel_name);
  EXPECT_EQ(0xff, cfg.channel_key[15]);
  EXPECT_STREQ("Sp\xC3\xA4ter", cfg.questions[0].replies[1]);
}

TEST(PagerConfig, EmptyDocumentKeepsBuildDefaults) {
  PagerConfig cfg;
  PagerCfgError err;
  ASSERT_TRUE(parse("# nothing\n\nquestions: []\n", cfg, err));
  EXPECT_EQ(0, cfg.flags);
  EXPECT_EQ(0, cfg.num_questions);
}

TEST(PagerConfig, SurrogatePairEscape) {
  PagerConfig cfg;
  PagerCfgError err;
  ASSERT_TRUE(parse("questions:\n  - text: Anrufen?\n    replies: [\"\\uD83D\\uDCDE\"]\n", cfg, err)) << err.msg;
  EXPECT_STREQ("\xF0\x9F\x93\x9E", cfg.questions[0].replies[0]);
}

struct BadCase {
  const char* yaml;
  int line;
  const char* msg_part;
};

TEST(PagerConfig, ReportsErrorsWithLine) {
  const BadCase cases[] = {
    { "nickname: Anna\nfoo: 1\n", 2, "unknown key 'foo'" },
    { "nickname: Anna\nnickname: Ben\n", 2, "duplicate key" },
    { "version: 2\n", 1, "unsupported version" },
    { "nickname: An:na\n", 1, "must not contain" },
    { "nickname: ThisNicknameIsWayTooLongForThePager\n", 1, "longer than 31" },
    { "channel:\n  key: 1234\n", 2, "32 hex" },
    { "channel:\n  key: zz3387e9c5cdea6ac9e5edbaa115cd72\n", 2, "32 hex" },
    { "channel:\n  name: Public\n", 2, "reserved" },
    { "channel: Pager\n", 1, "nested" },
    { "channel: {name: A, foo: b}\n", 1, "unknown key 'foo'" },
    { "channel: {name: A\n", 1, "missing '}'" },
    { "questions:\n  - text: A?\n", 2, "no replies" },
    { "questions:\n  - replies: [Ja]\n", 2, "without 'text'" },
    { "questions:\n  - text: A?\n    replies: [Ja]\n  - text: A?\n    replies: [Ja]\n", 4, "duplicate question" },
    { "questions:\n  - text: A?\n    replies: [Ja, Nein\n", 3, "missing ']'" },
    { "questions:\n  - text: \"A?\n    replies: [Ja]\n", 2, "closing quote" },
    { "questions:\n  - text: A?\n    replies: Ja\n", 3, "must be a list" },
    { "questions:\n  - text: A?\n    replies: [1, 2, 3, 4, 5, 6, 7]\n", 3, "more than 6 replies" },
    { "questions:\n  - text: This question text is definitely longer than forty bytes\n    replies: [Ja]\n", 2, "longer than 40" },
    { "questions:\n  - Angekommen?\n", 2, "expected 'text:'" },
    { "questions:\n  - text: A?\n      replies: [Ja]\n", 3, "inconsistent indentation" },
    { "nickname: Anna\n  foo: 1\n", 2, "unexpected indentation" },
    { "\tnickname: Anna\n", 1, "tabs" },
    { "nickname: |\n", 1, "plain text" },
    { "nickname: \"a\\nb\"\n", 1, "unsupported escape" },
    { "nickname: \"\\x07\"\n", 1, "control character" },
    { "nickname: \"\\xC3\"\n", 0, NULL },   // U+00C3 (valid) - just must not crash
    { "- Ja\n", 1, "unexpected list item" },
  };
  for (const BadCase& c : cases) {
    PagerConfig cfg;
    PagerCfgError err;
    bool ok = parse(c.yaml, cfg, err);
    if (c.msg_part == NULL) continue;
    EXPECT_FALSE(ok) << c.yaml;
    EXPECT_EQ(c.line, err.line) << c.yaml << " -> " << err.msg;
    EXPECT_NE(nullptr, strstr(err.msg, c.msg_part)) << c.yaml << " -> " << err.msg;
  }
}

TEST(PagerConfig, RejectsTooManyQuestions) {
  char yaml[2048] = "questions:\n";
  for (int i = 0; i <= PAGER_CFG_MAX_QUESTIONS; i++) {
    char q[64];
    snprintf(q, sizeof(q), "  - text: Frage %d\n    replies: [Ja]\n", i);
    strcat(yaml, q);
  }
  PagerConfig cfg;
  PagerCfgError err;
  EXPECT_FALSE(parse(yaml, cfg, err));
  EXPECT_NE(nullptr, strstr(err.msg, "more than 12 questions"));
}

TEST(PagerConfig, SerializeRoundTrip) {
  PagerConfig cfg, back;
  PagerCfgError err;
  ASSERT_TRUE(parse(FULL, cfg, err));

  uint8_t blob[PAGER_CFG_BLOB_MAX];
  int n = pagerCfgSerialize(cfg, blob, sizeof(blob));
  ASSERT_GT(n, 0);
  EXPECT_LT(n, 200);   // compact: only the used bytes are stored
  ASSERT_TRUE(pagerCfgDeserialize(blob, n, back));
  EXPECT_EQ(0, memcmp(&cfg, &back, sizeof(cfg)));

  EXPECT_FALSE(pagerCfgDeserialize(blob, n - 1, back));   // truncated
  blob[0] = 'X';
  EXPECT_FALSE(pagerCfgDeserialize(blob, n, back));       // bad magic
}

TEST(PagerConfig, WorstCaseFitsBlob) {
  PagerConfig cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.flags = PAGER_CFG_HAS_NICKNAME | PAGER_CFG_HAS_CHANNEL | PAGER_CFG_HAS_KEY;
  memset(cfg.nickname, 'n', PAGER_CFG_NAME_MAX);
  memset(cfg.channel_name, 'c', PAGER_CFG_NAME_MAX);
  cfg.num_questions = PAGER_CFG_MAX_QUESTIONS;
  for (int i = 0; i < PAGER_CFG_MAX_QUESTIONS; i++) {
    memset(cfg.questions[i].text, 'q', PAGER_CFG_TEXT_MAX);
    cfg.questions[i].num_replies = PAGER_CFG_MAX_REPLIES;
    for (int j = 0; j < PAGER_CFG_MAX_REPLIES; j++) memset(cfg.questions[i].replies[j], 'r', PAGER_CFG_TEXT_MAX);
  }
  uint8_t blob[PAGER_CFG_BLOB_MAX];
  EXPECT_EQ(PAGER_CFG_BLOB_MAX, pagerCfgSerialize(cfg, blob, sizeof(blob)));
}

TEST(PagerConfig, FindQuestion) {
  PagerConfig cfg;
  PagerCfgError err;
  ASSERT_TRUE(parse(FULL, cfg, err));
  EXPECT_EQ(&cfg.questions[0], pagerCfgFindQuestion(cfg, "Wann kommst du?"));
  EXPECT_EQ(nullptr, pagerCfgFindQuestion(cfg, "Wann kommst du"));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
