package com.android.saynaa.lang;

import androidx.annotation.NonNull;
import io.github.rosemoe.sora.lang.analysis.SimpleAnalyzeManager;
import io.github.rosemoe.sora.lang.styling.MappedSpans;
import io.github.rosemoe.sora.lang.styling.Styles;
import io.github.rosemoe.sora.widget.schemes.EditorColorScheme;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class SaynaaAnalyzeManager extends SimpleAnalyzeManager<Void> {
  private static final Set<String> KEYWORDS = new HashSet<>(Arrays.asList("and", "break", "case",
      "class", "continue", "delete", "default", "do", "else", "elif", "elseif", "end", "false",
      "for", "function", "if", "import", "in", "is", "local", "module", "null", "nil", "not", "or",
      "repeat", "return", "super", "switch", "then", "this", "true", "until", "while"));

  @Override
  protected Styles analyze(@NonNull StringBuilder text, Delegate<Void> delegate) {
    MappedSpans.Builder builder = new MappedSpans.Builder();
    int line = 0;
    int lineStart = 0;
    int length = text.length();

    for (int i = 0; i <= length; i++) {
      if (i == length || text.charAt(i) == '\n') {
        analyzeLine(text, lineStart, i, line, builder);
        line++;
        lineStart = i + 1;
      }
      if (delegate.isCancelled()) {
        return new Styles();
      }
    }

    builder.determine(Math.max(0, line - 1));
    return new Styles(builder.build());
  }

  private void analyzeLine(StringBuilder text, int start, int end, int line, MappedSpans.Builder builder) {
    builder.addIfNeeded(line, 0, EditorColorScheme.TEXT_NORMAL);
    int col = 0;
    int lineLength = end - start;

    while (col < lineLength) {
      char c = text.charAt(start + col);

      if (c == '#') {
        builder.addIfNeeded(line, col, EditorColorScheme.COMMENT);
        return;
      }

      if (c == '-' && col + 1 < lineLength && text.charAt(start + col + 1) == '-') {
        builder.addIfNeeded(line, col, EditorColorScheme.COMMENT);
        return;
      }

      if (c == '/' && col + 1 < lineLength && text.charAt(start + col + 1) == '/') {
        builder.addIfNeeded(line, col, EditorColorScheme.COMMENT);
        return;
      }

      if (c == '\'' || c == '"') {
        col = scanString(text, start, end, line, col, c, builder);
        continue;
      }

      if (isDigit(c) || (c == '.' && col + 1 < lineLength && isDigit(text.charAt(start + col + 1)))) {
        int numberEnd = scanNumber(text, start, end, col);
        builder.addIfNeeded(line, col, EditorColorScheme.LITERAL);
        builder.addIfNeeded(line, numberEnd, EditorColorScheme.TEXT_NORMAL);
        col = numberEnd;
        continue;
      }

      if (isWordStart(c)) {
        int wordEnd = scanWord(text, start, end, col);
        String word = text.substring(start + col, start + wordEnd);
        if (KEYWORDS.contains(word)) {
          builder.addIfNeeded(line, col, EditorColorScheme.KEYWORD);
          builder.addIfNeeded(line, wordEnd, EditorColorScheme.TEXT_NORMAL);
        }
        col = wordEnd;
        continue;
      }

      col++;
    }
  }

  private int scanString(StringBuilder text, int start, int end, int line, int col, char quote,
      MappedSpans.Builder builder) {
    builder.addIfNeeded(line, col, EditorColorScheme.LITERAL);
    col++;
    boolean escaped = false;
    while (start + col < end) {
      char ch = text.charAt(start + col);
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == quote) {
        col++;
        break;
      }
      col++;
    }
    builder.addIfNeeded(line, col, EditorColorScheme.TEXT_NORMAL);
    return col;
  }

  private int scanNumber(StringBuilder text, int start, int end, int col) {
    int abs = start + col;
    if (abs + 1 < end && text.charAt(abs) == '0') {
      char next = text.charAt(abs + 1);
      if (next == 'x' || next == 'X') {
        col += 2;
        while (start + col < end && isHexDigit(text.charAt(start + col))) {
          col++;
        }
        return col;
      }
      if (next == 'b' || next == 'B') {
        col += 2;
        while (start + col < end && isBinaryDigit(text.charAt(start + col))) {
          col++;
        }
        return col;
      }
    }

    boolean sawDot = false;
    boolean sawExp = false;
    while (start + col < end) {
      char ch = text.charAt(start + col);
      if (isDigit(ch)) {
        col++;
        continue;
      }
      if (ch == '.' && !sawDot && !sawExp) {
        sawDot = true;
        col++;
        continue;
      }
      if ((ch == 'e' || ch == 'E') && !sawExp) {
        sawExp = true;
        col++;
        if (start + col < end) {
          char sign = text.charAt(start + col);
          if (sign == '+' || sign == '-') {
            col++;
          }
        }
        continue;
      }
      break;
    }
    return col;
  }

  private int scanWord(StringBuilder text, int start, int end, int col) {
    while (start + col < end) {
      char ch = text.charAt(start + col);
      if (isWordPart(ch)) {
        col++;
      } else {
        break;
      }
    }
    return col;
  }

  private boolean isWordStart(char c) {
    return c == '_' || Character.isLetter(c);
  }

  private boolean isWordPart(char c) {
    return c == '_' || Character.isLetterOrDigit(c);
  }

  private boolean isDigit(char c) {
    return c >= '0' && c <= '9';
  }

  private boolean isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
  }

  private boolean isBinaryDigit(char c) {
    return c == '0' || c == '1';
  }
}
