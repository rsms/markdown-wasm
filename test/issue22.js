// https://github.com/rsms/markdown-wasm/issues/22
const { checkHTMLResult, exit } = require('./testutil');

checkHTMLResult('DISABLE_HEADLINE_ANCHOR', `# Test1
## Test2
### Test3
#### Test4
`, `<h1>Test1</h1>
<h2>Test2</h2>
<h3>Test3</h3>
<h4>Test4</h4>\n`, {
  disableHeadlineAnchors: true,
});

checkHTMLResult(
  'ENABLE_HEADLINE_ANCHOR',
  `# Test1
## Test2
### Test3
#### Test4`,
  `<h1><a id="test1" class="anchor" aria-hidden="true" href="#test1"></a>Test1</h1>
<h2><a id="test2" class="anchor" aria-hidden="true" href="#test2"></a>Test2</h2>
<h3><a id="test3" class="anchor" aria-hidden="true" href="#test3"></a>Test3</h3>
<h4><a id="test4" class="anchor" aria-hidden="true" href="#test4"></a>Test4</h4>\n`,
  {
    disableHeadlineAnchors: false,
  }
);

exit();
