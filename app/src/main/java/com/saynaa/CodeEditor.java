package com.saynaa;

import android.content.Context;
import android.graphics.Typeface;
import android.util.AttributeSet;
import com.saynaa.lang.SaynaaLanguage;
import io.github.rosemoe.sora.widget.schemes.SchemeDarcula;
import io.github.rosemoe.sora.widget.schemes.SchemeEclipse;

public class CodeEditor extends io.github.rosemoe.sora.widget.CodeEditor {
  public CodeEditor(Context context) {
    super(context);
    initDefaults();
  }

  public CodeEditor(Context context, AttributeSet attrs) {
    super(context, attrs);
    initDefaults();
  }

  public CodeEditor(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
    initDefaults();
  }

  private void initDefaults() {
    setTypefaceText(Typeface.MONOSPACE);
    setTextSize(16);
    setLineNumberEnabled(true);
    setHighlightCurrentLine(true);
    setHighlightBracketPair(true);
    setWordwrap(false);
    setEditorLanguage(new SaynaaLanguage());
  }

  public void setDark(boolean dark) {
    if (dark) {
      setColorScheme(new SchemeDarcula());
    } else {
      setColorScheme(new SchemeEclipse());
    }
  }
}
