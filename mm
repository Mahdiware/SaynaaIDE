     * @see #INTERRUPTION_LEVEL_STRONG
     * @see #INTERRUPTION_LEVEL_SLIGHT
     * @see #INTERRUPTION_LEVEL_NONE
     */
    int getInterruptionLevel();

    /**
     * Request to auto-complete the code at the given {@code position}.
     * Note that if you did not correctly set the spans for the text, the auto-completion will not be triggered.
     * This is called in a worker thread other than UI thread.
     *
     * @param content        Read-only reference of content
     * @param position       The position for auto-complete
     * @param publisher      The publisher used to update items
     * @param extraArguments Arguments set by {@link CodeEditor#setText(CharSequence, Bundle)}
     * @throws io.github.rosemoe.sora.lang.completion.CompletionCancelledException This thread can be abandoned
     *                                                                             by the editor framework because the auto-completion items of
     *                                                                             this invocation are no longer needed by the user. This can either be thrown
     *                                                                             by {@link ContentReference} or {@link CompletionPublisher}.
     *                                                                             How the exceptions will be thrown is according to
     *                                                                             your settings: {@link #getInterruptionLevel()}
     * @see ContentReference
     * @see CompletionPublisher
     * @see #getInterruptionLevel()
     * @see CompletionHelper#checkCancelled()
     */
    @WorkerThread
    void requireAutoComplete(@NonNull ContentReference content, @NonNull CharPosition position,
                             @NonNull CompletionPublisher publisher,
                             @NonNull Bundle extraArguments) throws CompletionCancelledException;

    /**
     * Get delta indent spaces count.
     *
     * @param content Content of given line.
     * @param line    0-indexed line number. The indentation is applied on line index: {@code line + 1}.
     * @param column  Column on the given line, where a line separator is inserted.
     * @return Delta count of indent spaces. It can be a negative/positive number or zero.
     */
    @UiThread
    int getIndentAdvance(@NonNull ContentReference content, int line, int column);

    /**
     * Get delta indent spaces count.
     *
     * @param content          Content of given line.
     * @param line             0-indexed line number. The indentation is applied on line index: {@code line + 1}.
     * @param column           Column on the given line, where a line separator is inserted.
     * @param spaceCountOnLine The number of spaces on {@code line}.
     * @param tabCountOnLine   The number of tabs on {@code line}.
     * @return Delta count of indent spaces. It can be a negative/positive number or zero.
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:115:            @NonNull String prefix, @NonNull CompletionPublisher publisher, @Nullable Identifiers userIdentifiers) {
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:118:                reference, position, createCompletionItemList(prefix, userIdentifiers)
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:131:            @NonNull String prefix, @Nullable Identifiers userIdentifiers
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:172:        if (userIdentifiers != null) {
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:175:            userIdentifiers.filterIdentifiers(prefix, dest);
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:193:            @NonNull String prefix, @NonNull CompletionPublisher publisher, @Nullable Identifiers userIdentifiers) {
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:196:        publisher.addItems(createCompletionItemList(prefix, userIdentifiers));
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:203:     * @see IdentifierAutoComplete.DisposableIdentifiers
sora-editor/src/main/java/io/github/rosemoe/sora/lang/completion/IdentifierAutoComplete.java:226:    public static class DisposableIdentifiers implements Identifiers {
