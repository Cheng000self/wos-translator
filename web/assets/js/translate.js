// 多文件支持
let selectedFiles = [];  // [{file, content, count}]
let models = [];
let maxUploadFiles = 10;  // 默认值，会从设置中加载

// 初始化
document.addEventListener('DOMContentLoaded', async () => {
    await loadSettings();
    await loadModels();
    setupEventListeners();
});

// 加载系统设置
async function loadSettings() {
    try {
        const response = await apiCall('GET', '/api/settings');
        if (response.maxUploadFiles) {
            maxUploadFiles = response.maxUploadFiles;
        }
    } catch (error) {
        // 使用默认值
        console.log('Using default maxUploadFiles:', maxUploadFiles);
    }
}

// 加载模型列表
async function loadModels() {
    try {
        const response = await apiCall('GET', '/api/models');
        models = response;
        
        const select = document.getElementById('modelSelect');
        select.innerHTML = '<option value="">选择已保存的模型</option>';
        select.innerHTML += '<option value="custom">自定义配置...</option>';
        
        models.forEach(model => {
            const option = document.createElement('option');
            option.value = model.id;
            option.textContent = model.name;
            select.appendChild(option);
        });
    } catch (error) {
        showToast('加载模型列表失败: ' + error.message, 'error');
    }
}

// 设置事件监听器
function setupEventListeners() {
    const dropZone = document.getElementById('dropZone');
    const fileInput = document.getElementById('fileInput');
    const clearAllFiles = document.getElementById('clearAllFiles');
    const modelSelect = document.getElementById('modelSelect');
    const testConnection = document.getElementById('testConnection');
    const createTask = document.getElementById('createTask');
    
    // 点击上传
    dropZone.addEventListener('click', () => fileInput.click());
    
    // 文件选择
    fileInput.addEventListener('change', handleFileSelect);
    
    // 拖拽上传
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
        
        const files = Array.from(e.dataTransfer.files);
        handleMultipleFiles(files);
    });
    
    // 清空全部文件
    clearAllFiles.addEventListener('click', () => {
        selectedFiles = [];
        fileInput.value = '';
        updateFileList();
        updateCreateButton();
    });
    
    // 模型选择
    modelSelect.addEventListener('change', () => {
        const customSection = document.getElementById('customModelSection');
        if (modelSelect.value === 'custom') {
            customSection.classList.remove('hidden');
        } else {
            customSection.classList.add('hidden');
        }
        updateCreateButton();
    });
    
    // 测试连接
    testConnection.addEventListener('click', testModelConnection);
    
    // 创建任务
    createTask.addEventListener('click', createTranslationTask);
    
    // 翻译选项变化
    document.getElementById('translateTitle').addEventListener('change', updateCreateButton);
    document.getElementById('translateAbstract').addEventListener('change', updateCreateButton);
}

// 处理文件选择
function handleFileSelect(e) {
    const files = Array.from(e.target.files);
    handleMultipleFiles(files);
}

// 处理多个文件
async function handleMultipleFiles(files) {
    for (const file of files) {
        // 检查文件数量限制
        if (selectedFiles.length >= maxUploadFiles) {
            showToast(`每个任务最多上传 ${maxUploadFiles} 个文件`, 'warning');
            break;
        }
        
        // 检查是否已存在
        if (selectedFiles.some(f => f.file.name === file.name)) {
            showToast(`文件 ${file.name} 已存在`, 'warning');
            continue;
        }
        
        // 验证文件
        const validation = validateHtmlFile(file);
        if (!validation.valid) {
            showToast(`${file.name}: ${validation.error}`, 'error');
            continue;
        }
        
        try {
            // 读取文件内容
            const content = await readFileContent(file);
            
            // 验证WoS格式
            const wosValidation = validateWosFormat(content);
            if (!wosValidation.valid) {
                showToast(`${file.name}: ${wosValidation.error}`, 'error');
                continue;
            }
            
            // 估算文献数量
            const count = estimateLiteratureCount(content);
            
            selectedFiles.push({
                file: file,
                content: content,
                count: count
            });
        } catch (error) {
            showToast(`读取 ${file.name} 失败: ${error.message}`, 'error');
        }
    }
    
    updateFileList();
    updateCreateButton();
}

// 更新文件列表显示
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
        html += `
            <div class="bg-blue-50 border border-blue-200 rounded-lg p-3 flex items-center justify-between">
                <div class="flex items-center space-x-3">
                    <span class="text-sm font-medium text-blue-600 w-6">${index + 1}.</span>
                    <svg class="w-6 h-6 text-blue-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"></path>
                    </svg>
                    <div>
                        <p class="text-sm font-medium text-slate-900">${item.file.name}</p>
                        <p class="text-xs text-slate-500">${formatFileSize(item.file.size)} · 约 ${item.count} 篇文献</p>
                    </div>
                </div>
                <button onclick="removeFile(${index})" class="text-red-600 hover:text-red-700 cursor-pointer p-1">
                    <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>
                    </svg>
                </button>
            </div>
        `;
    });
    
    fileItems.innerHTML = html;
    
    // 显示文件数量和限制
    let countText = `预估共 ${total} 篇文献（${selectedFiles.length}/${maxUploadFiles} 个文件）`;
    if (selectedFiles.length >= maxUploadFiles) {
        countText += ' <span class="text-orange-600">已达上限</span>';
    }
    totalCount.innerHTML = countText;
}

// 移除单个文件
function removeFile(index) {
    selectedFiles.splice(index, 1);
    updateFileList();
    updateCreateButton();
}

// 测试模型连接
async function testModelConnection() {
    const url = document.getElementById('modelUrl').value.trim();
    const apiKey = document.getElementById('modelApiKey').value.trim();
    const modelId = document.getElementById('modelId').value.trim();
    const temperature = parseFloat(document.getElementById('modelTemperature').value);
    const systemPrompt = document.getElementById('modelPrompt').value.trim();
    
    if (!url || !apiKey || !modelId) {
        showToast('请填写完整的模型配置', 'warning');
        return;
    }
    
    try {
        showLoading('正在测试连接...');
        
        const result = await apiCall('POST', '/api/models/test', {
            url,
            apiKey,
            modelId,
            temperature,
            systemPrompt: systemPrompt || '你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。'
        });
        
        hideLoading();
        
        if (result.success) {
            showToast('连接测试成功！', 'success');
        } else {
            showToast('连接测试失败: ' + (result.error || '未知错误'), 'error');
        }
    } catch (error) {
        hideLoading();
        showToast('连接测试失败: ' + error.message, 'error');
    }
}

// 创建翻译任务
async function createTranslationTask() {
    if (selectedFiles.length === 0) {
        showToast('请先上传文件', 'warning');
        return;
    }
    
    const modelSelect = document.getElementById('modelSelect');
    const selectedModelId = modelSelect.value;
    
    if (!selectedModelId) {
        showToast('请选择模型', 'warning');
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
        
        let modelConfig;
        
        if (selectedModelId === 'custom') {
            const url = document.getElementById('modelUrl').value.trim();
            const apiKey = document.getElementById('modelApiKey').value.trim();
            const modelId = document.getElementById('modelId').value.trim();
            const temperature = parseFloat(document.getElementById('modelTemperature').value);
            const systemPrompt = document.getElementById('modelPrompt').value.trim();
            
            if (!url || !apiKey || !modelId) {
                hideLoading();
                showToast('请填写完整的模型配置', 'warning');
                return;
            }
            
            modelConfig = {
                url,
                apiKey,
                modelId,
                temperature,
                systemPrompt: systemPrompt || '你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。'
            };
        } else {
            const model = models.find(m => m.id === selectedModelId);
            if (!model) {
                hideLoading();
                showToast('模型不存在', 'error');
                return;
            }
            
            modelConfig = {
                url: model.url,
                apiKey: model.apiKey,
                modelId: model.modelId,
                temperature: model.temperature,
                systemPrompt: model.systemPrompt
            };
        }
        
        // 构建请求数据
        const requestData = {
            taskName: document.getElementById('taskName').value.trim(),
            translateTitle,
            translateAbstract,
            modelConfig
        };
        
        if (selectedFiles.length === 1) {
            // 单文件
            requestData.fileName = selectedFiles[0].file.name;
            requestData.htmlContent = selectedFiles[0].content;
        } else {
            // 多文件
            requestData.fileNames = selectedFiles.map(f => f.file.name);
            requestData.htmlContents = selectedFiles.map(f => f.content);
        }
        
        const result = await apiCall('POST', '/api/tasks', requestData);
        
        hideLoading();
        
        if (result.success) {
            showToast('任务创建成功！', 'success');
            setTimeout(() => {
                window.location.href = '/queue.html';
            }, 1000);
        } else {
            showToast('任务创建失败: ' + (result.error || '未知错误'), 'error');
        }
    } catch (error) {
        hideLoading();
        showToast('任务创建失败: ' + error.message, 'error');
    }
}

// 更新创建按钮状态
function updateCreateButton() {
    const createTask = document.getElementById('createTask');
    const modelSelect = document.getElementById('modelSelect');
    const translateTitle = document.getElementById('translateTitle').checked;
    const translateAbstract = document.getElementById('translateAbstract').checked;
    
    const hasFile = selectedFiles.length > 0;
    const hasModel = modelSelect.value !== '';
    const hasOption = translateTitle || translateAbstract;
    
    createTask.disabled = !(hasFile && hasModel && hasOption);
}
