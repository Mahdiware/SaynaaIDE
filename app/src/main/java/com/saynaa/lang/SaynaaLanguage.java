package com.saynaa.lang;

import android.os.Bundle;
import androidx.annotation.NonNull;
import io.github.rosemoe.sora.lang.EmptyLanguage;
import io.github.rosemoe.sora.lang.analysis.AnalyzeManager;
import io.github.rosemoe.sora.lang.completion.CompletionItem;
import io.github.rosemoe.sora.lang.completion.CompletionItemKind;
import io.github.rosemoe.sora.lang.completion.CompletionPublisher;
import io.github.rosemoe.sora.lang.completion.SimpleCompletionItem;
import io.github.rosemoe.sora.text.CharPosition;
import io.github.rosemoe.sora.text.ContentReference;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class SaynaaLanguage extends EmptyLanguage {
  private final SaynaaAnalyzeManager analyzeManager = new SaynaaAnalyzeManager();

  @NonNull
  @Override
  public AnalyzeManager getAnalyzeManager() {
    return analyzeManager;
  }

  @Override
  public void requireAutoComplete(@NonNull ContentReference content, @NonNull CharPosition position,
      @NonNull CompletionPublisher publisher, @NonNull Bundle extraArguments) {
    String prefix = getPrefix(content, position);

    if (prefix.isEmpty()) {
      return;
    }

    List<CompletionItem> items = new ArrayList<>();

    addKeywords(prefix, items);
    addFunctions(prefix, items);
    addIdentifiers(content, position, prefix, items);

    publisher.addItems(items);
  }

  private void addKeywords(String prefix, List<CompletionItem> items) {
    for (String keyword : SaynaaCompletionItems.KEYWORDS) {
      if (matches(keyword, prefix)) {
        items.add(new SimpleCompletionItem(keyword, "Keyword", prefix.length(), keyword)
                .kind(CompletionItemKind.Keyword));
      }
    }
  }

  private void addFunctions(String prefix, List<CompletionItem> items) {
    for (String function : SaynaaCompletionItems.FUNCTIONS) {
      if (matches(function, prefix)) {
        items.add(new SimpleCompletionItem(function, "Function", prefix.length(), function)
                .kind(CompletionItemKind.Function));
      }
    }
  }

  private void addIdentifiers(
      ContentReference content, CharPosition position, String prefix, List<CompletionItem> items) {
    Set<String> identifiers = new HashSet<>();

    for (int line = 0; line < content.getLineCount(); line++) {
      String text = content.getLine(line).toString();

      int i = 0;

      while (i < text.length()) {
        char c = text.charAt(i);

        if (c == '_' || Character.isLetter(c)) {
          int start = i;

          i++;

          while (i < text.length()) {
            char ch = text.charAt(i);

            if (ch == '_' || Character.isLetterOrDigit(ch)) {
              i++;
            } else {
              break;
            }
          }

          int end = i;

          /*
           * Don't add the identifier currently being typed.
           *
           * moha = "mohamed"
           * mo|
           *
           * The "mo" currently under the cursor is ignored.
           */
          if (line == position.line && position.column >= start && position.column <= end) {
            continue;
          }

          String word = text.substring(start, end);

          if (matches(word, prefix)) {
            identifiers.add(word);
          }

        } else {
          i++;
        }
      }
    }

    for (String identifier : identifiers) {
      if (isKeyword(identifier) || isFunction(identifier)) {
        continue;
      }

      items.add(new SimpleCompletionItem(identifier, "Identifier", prefix.length(), identifier)
              .kind(CompletionItemKind.Identifier));
    }
  }

  private boolean matches(String value, String prefix) {
    return value.regionMatches(true, 0, prefix, 0, prefix.length());
  }

  private boolean isKeyword(String value) {
    for (String keyword : SaynaaCompletionItems.KEYWORDS) {
      if (keyword.equalsIgnoreCase(value)) {
        return true;
      }
    }

    return false;
  }

  private boolean isFunction(String value) {
    for (String function : SaynaaCompletionItems.FUNCTIONS) {
      if (function.equalsIgnoreCase(value)) {
        return true;
      }
    }

    return false;
  }

  private String getPrefix(ContentReference content, CharPosition position) {
    String line = content.getLine(position.line).toString();

    int column = Math.min(position.column, line.length());

    int start = column;

    while (start > 0) {
      char c = line.charAt(start - 1);

      if (!Character.isLetterOrDigit(c) && c != '_') {
        break;
      }

      start--;
    }

    return line.substring(start, column);
  }

  @Override
  public void destroy() {
    analyzeManager.destroy();
  }
}