// 多文件支持
let selectedFiles = [];  // [{file, content, count}]
let models = [];
let maxUploadFiles = 10;
let maxTranslationThreads = 1;
let maxModelsPerTask = 5;
let selectedModels = [];  // [{modelId, name, threads, provider, ...modelData}]

document.addEventListener('DOMContentLoaded', async () => {
    await loadSettings();
    await loadModels();
    setupEventListeners();
});

async function loadSettings() {
    try {
        const response = await apiCall('GET', '/api/settings');
        if (response.maxUploadFiles) maxUploadFiles = response.maxUploadFiles;
        if (response.maxTranslationThreads) maxTranslationThreads = response.maxTranslationThreads;
        if (response.maxModelsPerTask) maxModelsPerTask = response.maxModelsPerTask;
    } catch (error) {
        console.log('Using default settings');
    }
}

async function loadModels() {
    try {
        const response = await apiCall('GET', '/api/models');
        models = response;
        const select = document.getElementById('modelSelect');
        select.innerHTML = '<option value="">选择模型...</option>';
        models.forEach(model => {
            const option = document.createElement('option');
            option.value = model.id;
            option.textContent = model.name;
            select.appendChild(option);
        });
        document.getElementById('modelThreads').max = maxTranslationThreads;
        updateModelLimitInfo();
    } catch (error) {
        showToast('加载模型列表失败: ' + error.message, 'error');
    }
}

function updateModelLimitInfo() {
    document.getElementById('modelLimitInfo').textContent = 
        selectedModels.length + '/' + maxModelsPerTask + ' 个模型';
}

function getProviderTagClass(p) {
    if (p === 'xiaomi') return 'bg-orange-100 text-orange-700';
    if (p === 'minimax') return 'bg-purple-100 text-purple-700';
    return 'bg-blue-100 text-blue-700';
}

function getProviderLabel(p) {
    if (p === 'xiaomi') return 'Xiaomi';
    if (p === 'minimax') return 'MiniMAX';
    return 'OpenAI';
}

function setupEventListeners() {
    const dropZone = document.getElementById('dropZone');
    const fileInput = document.getElementById('fileInput');
    const clearAllFiles = document.getElementById('clearAllFiles');
    const addModelBtn = document.getElementById('addModelBtn');
    const createTask = document.getElementById('createTask');

    dropZone.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', handleFileSelect);

    dropZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropZone.classList.add('border-blue-500', 'bg-blue-50');
    });
    dropZone.addEventListener('dragleave', () => {
        dropZone.classList.remove('border-blue-500', 'bg-blue-50');
    });
    dropZone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropZone.classList.remove('border-blue-500', 'bg-blue-50');
        handleMultipleFiles(Array.from(e.dataTransfer.files));
    });

    clearAllFiles.addEventListener('click', () => {
        selectedFiles = [];
        fileInput.value = '';
        updateFileList();
        updateCreateButton();
    });

    addModelBtn.addEventListener('click', addModel);
    createTask.addEventListener('click', createTranslationTask);

    document.getElementById('translateTitle').addEventListener('change', updateCreateButton);
    document.getElementById('translateAbstract').addEventListener('change', updateCreateButton);
}

function addModel() {
    const select = document.getElementById('modelSelect');
    const modelId = select.value;
    if (!modelId) {
        showToast('请选择模型', 'warning');
        return;
    }
    if (selectedModels.length >= maxModelsPerTask) {
        showToast('已达到最大模型数量 (' + maxModelsPerTask + ')', 'warning');
        return;
    }
    // 检查是否已添加相同模型
    if (selectedModels.some(m => m.modelId === modelId)) {
        showToast('该模型已添加，同一模型只能添加一次', 'warning');
        return;
    }
    const model = models.find(m => m.id === modelId);
    if (!model) return;

    const threads = Math.min(
        Math.max(1, parseInt(document.getElementById('modelThreads').value) || 1),
        maxTranslationThreads
    );

    selectedModels.push({
        modelId: model.id,
        name: model.name,
        threads: threads,
        provider: model.provider || 'openai',
        url: model.url,
        apiKey: model.apiKey,
        modelIdStr: model.modelId,
        temperature: model.temperature,
        systemPrompt: model.systemPrompt,
        enableThinking: model.enableThinking || false,
        autoAppendPath: model.autoAppendPath !== false
    });

    renderSelectedModels();
    updateCreateButton();
    updateModelLimitInfo();
}

function removeModel(index) {
    selectedModels.splice(index, 1);
    renderSelectedModels();
    updateCreateButton();
    updateModelLimitInfo();
}

function updateModelThreads(index, value) {
    const threads = Math.min(Math.max(1, parseInt(value) || 1), maxTranslationThreads);
    selectedModels[index].threads = threads;
}

function renderSelectedModels() {
    const container = document.getElementById('selectedModels');
    const hint = document.getElementById('noModelHint');

    if (selectedModels.length === 0) {
        container.innerHTML = '';
        hint.classList.remove('hidden');
        return;
    }
    hint.classList.add('hidden');

    container.innerHTML = selectedModels.map((m, i) => {
        const tagClass = getProviderTagClass(m.provider);
        const providerLabel = getProviderLabel(m.provider);
        return '<div class="flex items-center justify-between bg-slate-50 border border-slate-200 rounded-lg px-4 py-3">' +
            '<div class="flex items-center space-x-3 flex-1 min-w-0">' +
                '<span class="text-sm font-medium text-slate-400 w-5">' + (i + 1) + '</span>' +
                '<span class="px-2 py-0.5 text-xs ' + tagClass + ' rounded-full">' + providerLabel + '</span>' +
                '<span class="text-sm font-medium text-slate-900 truncate">' + m.name + '</span>' +
            '</div>' +
            '<div class="flex items-center space-x-3 flex-shrink-0">' +
                '<div class="flex items-center space-x-1">' +
                    '<span class="text-xs text-slate-500">线程</span>' +
                    '<input type="number" value="' + m.threads + '" min="1" max="' + maxTranslationThreads + '" ' +
                        'onchange="updateModelThreads(' + i + ', this.value)" ' +
                        'class="w-14 px-2 py-1 text-sm border border-gray-300 rounded text-center">' +
                '</div>' +
                '<button onclick="removeModel(' + i + ')" class="p-1 text-red-500 hover:bg-red-50 rounded cursor-pointer">' +
                    '<svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path></svg>' +
                '</button>' +
            '</div>' +
        '</div>';
    }).join('');
}

function handleFileSelect(e) {
    handleMultipleFiles(Array.from(e.target.files));
}

async function handleMultipleFiles(files) {
    for (const file of files) {
        if (selectedFiles.length >= maxUploadFiles) {
            showToast('每个任务最多上传 ' + maxUploadFiles + ' 个文件', 'warning');
            break;
        }
        if (selectedFiles.some(f => f.file.name === file.name)) {
            showToast('文件 ' + file.name + ' 已存在', 'warning');
            continue;
        }
        const validation = validateHtmlFile(file);
        if (!validation.valid) {
            showToast(file.name + ': ' + validation.error, 'error');
            continue;
        }
        try {
            const content = await readFileContent(file);
            const wosValidation = validateWosFormat(content);
            if (!wosValidation.valid) {
                showToast(file.name + ': ' + wosValidation.error, 'error');
                continue;
            }
            const count = estimateLiteratureCount(content);
            selectedFiles.push({ file, content, count });
        } catch (error) {
            showToast('读取 ' + file.name + ' 失败: ' + error.message, 'error');
        }
    }
    updateFileList();
    updateCreateButton();
}

function updateFileList() {
    const fileList = document.getElementById('fileList');
    const fileItems = document.getElementById('fileItems');
    const totalCount = document.getElementById('totalLiteratureCount');
    if (selectedFiles.length === 0) {
        fileList.classList.add('hidden');
        return;
    }
    fileList.classList.remove('hidden');
    let html = '';
    let total = 0;
    selectedFiles.forEach((item, index) => {
        total += item.count;
        html += '<div class="bg-blue-50 border border-blue-200 rounded-lg p-3 flex items-center justify-between">' +
            '<div class="flex items-center space-x-3">' +
                '<span class="text-sm font-medium text-blue-600 w-6">' + (index + 1) + '.</span>' +
                '<svg class="w-6 h-6 text-blue-600" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"></path></svg>' +
                '<div><p class="text-sm font-medium text-slate-900">' + item.file.name + '</p>' +
                '<p class="text-xs text-slate-500">' + formatFileSize(item.file.size) + ' · 约 ' + item.count + ' 篇文献</p></div>' +
            '</div>' +
            '<button onclick="removeFile(' + index + ')" class="text-red-600 hover:text-red-700 cursor-pointer p-1">' +
                '<svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path></svg>' +
            '</button></div>';
    });
    fileItems.innerHTML = html;
    let countText = '预估共 ' + total + ' 篇文献（' + selectedFiles.length + '/' + maxUploadFiles + ' 个文件）';
    if (selectedFiles.length >= maxUploadFiles) countText += ' <span class="text-orange-600">已达上限</span>';
    totalCount.innerHTML = countText;
}

function removeFile(index) {
    selectedFiles.splice(index, 1);
    updateFileList();
    updateCreateButton();
}

async function createTranslationTask() {
    if (selectedFiles.length === 0) {
        showToast('请先上传文件', 'warning');
        return;
    }
    if (selectedModels.length === 0) {
        showToast('请添加至少一个翻译模型', 'warning');
        return;
    }
    const translateTitle = document.getElementById('translateTitle').checked;
    const translateAbstract = document.getElementById('translateAbstract').checked;
    if (!translateTitle && !translateAbstract) {
        showToast('请至少选择一个翻译选项', 'warning');
        return;
    }

    try {
        showLoading('正在创建任务...');

        // 构建模型配置数组
        const modelConfigs = selectedModels.map(m => ({
            url: m.url,
            apiKey: m.apiKey,
            modelId: m.modelIdStr,
            name: m.name,
            temperature: m.temperature,
            systemPrompt: m.systemPrompt || '',
            provider: m.provider,
            enableThinking: m.enableThinking,
            autoAppendPath: m.autoAppendPath,
            threads: m.threads
        }));

        // 兼容：同时发送单模型的modelConfig
        const firstModel = selectedModels[0];
        const modelConfig = {
            url: firstModel.url,
            apiKey: firstModel.apiKey,
            modelId: firstModel.modelIdStr,
            temperature: firstModel.temperature,
            systemPrompt: firstModel.systemPrompt || '',
            provider: firstModel.provider,
            enableThinking: firstModel.enableThinking,
            autoAppendPath: firstModel.autoAppendPath
        };

        const requestData = {
            taskName: document.getElementById('taskName').value.trim(),
            translateTitle,
            translateAbstract,
            modelConfig,
            modelConfigs
        };

        if (selectedFiles.length === 1) {
            requestData.fileName = selectedFiles[0].file.name;
            requestData.htmlContent = selectedFiles[0].content;
        } else {
            requestData.fileNames = selectedFiles.map(f => f.file.name);
            requestData.htmlContents = selectedFiles.map(f => f.content);
        }

        const result = await apiCall('POST', '/api/tasks', requestData);
        hideLoading();

        if (result.success) {
            showToast('任务创建成功！', 'success');
            setTimeout(() => { window.location.href = '/queue.html'; }, 1000);
        } else {
            showToast('任务创建失败: ' + (result.error || '未知错误'), 'error');
        }
    } catch (error) {
        hideLoading();
        showToast('任务创建失败: ' + error.message, 'error');
    }
}

function updateCreateButton() {
    const createTask = document.getElementById('createTask');
    const translateTitle = document.getElementById('translateTitle').checked;
    const translateAbstract = document.getElementById('translateAbstract').checked;
    const hasFile = selectedFiles.length > 0;
    const hasModel = selectedModels.length > 0;
    const hasOption = translateTitle || translateAbstract;
    createTask.disabled = !(hasFile && hasModel && hasOption);
}
